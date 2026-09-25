#include "pb_internal.h"

// SOCKS5: CONNECT (IPv4/IPv6/domain) and UDP ASSOCIATE.

// Read and validate a SOCKS5 CONNECT reply accoring to RFC 1928
// Goal -  The proxy picks the BND.ADDR
// type in its reply  seperatly of the request's ATYP few proxies answer an IPv6 CONNECT with a 4-byte IPv4 0.0.0.0 BND.addr
//  parse the 4-byte header
// (VER REP RSV ATYP) and then drain the variable-length BND.ADDR + BND.PORT by ATYP
int socks5_read_connect_reply(SOCKET s, int *reply)
{
    unsigned char hdr[4];
    int len = recv_n(s, (char*)hdr, 4);
    if (reply) *reply = (len >= 2) ? hdr[1] : -1;
    if (len != 4 || hdr[0] != SOCKS5_VERSION || hdr[1] != 0x00) return -1;

    int drain;
    if      (hdr[3] == SOCKS5_ATYP_IPV4) drain = 4 + 2;
    else if (hdr[3] == SOCKS5_ATYP_IPV6) drain = 16 + 2;
    else if (hdr[3] == SOCKS5_ATYP_DOMAIN)
    {
        unsigned char dlen;
        if (recv_n(s, (char*)&dlen, 1) != 1) return -1;
        drain = (int)dlen + 2;
    }
    else return -1;   // unknown ATYP

    unsigned char scratch[270];   // max drain = 255 + 2 (domain) < 270
    if (drain > 0 && recv_n(s, (char*)scratch, drain) != drain) return -1;
    return 0;
}

// KokoroBox uses an unauthenticated local SOCKS5 endpoint. Never negotiate
// RFC 1929 username/password authentication over a cleartext TCP connection.
static int socks5_negotiate_no_auth(SOCKET s)
{
    const unsigned char greeting[3] = { SOCKS5_VERSION, 0x01, SOCKS5_AUTH_NONE };
    unsigned char reply[2];

    if (send_all(s, (const char*)greeting, sizeof(greeting)) != sizeof(greeting))
        return -1;
    if (recv_n(s, (char*)reply, sizeof(reply)) != sizeof(reply) ||
        reply[0] != SOCKS5_VERSION || reply[1] != SOCKS5_AUTH_NONE)
        return -1;
    return 0;
}

// SOCKS5 CONNECT with ATYP_DOMAIN
int socks5_connect_domain(SOCKET s, const char *hostname, UINT16 dest_port)
{
    unsigned char request[7 + 255];
    size_t hlen = strnlen_s(hostname, 255);
    if (hlen == 0 || hlen > 255 || socks5_negotiate_no_auth(s) != 0)
        return -1;

    request[0] = SOCKS5_VERSION;
    request[1] = SOCKS5_CMD_CONNECT;
    request[2] = 0x00;
    request[3] = SOCKS5_ATYP_DOMAIN;
    request[4] = (unsigned char)hlen;
    memcpy(&request[5], hostname, hlen);
    request[5 + hlen] = (dest_port >> 8) & 0xFF;
    request[6 + hlen] = (dest_port >> 0) & 0xFF;
    int req_len = (int)(7 + hlen);

    if (send_all(s, (const char*)request, req_len) != req_len) return -1;

    int reply;
    if (socks5_read_connect_reply(s, &reply) != 0)
    {
        log_message("SOCKS5 domain: CONNECT failed (reply=%d)", reply);
        return -1;
    }
    return 0;
}

int socks5_connect(SOCKET s, UINT32 dest_ip, UINT16 dest_port)
{
    unsigned char request[10];
    if (socks5_negotiate_no_auth(s) != 0)
    {
        log_message("SOCKS5: No-auth negotiation failed");
        return -1;
    }

    request[0] = SOCKS5_VERSION;
    request[1] = SOCKS5_CMD_CONNECT;
    request[2] = 0x00;
    request[3] = SOCKS5_ATYP_IPV4;
    request[4] = (dest_ip >> 0) & 0xFF;
    request[5] = (dest_ip >> 8) & 0xFF;
    request[6] = (dest_ip >> 16) & 0xFF;
    request[7] = (dest_ip >> 24) & 0xFF;
    request[8] = (dest_port >> 8) & 0xFF;
    request[9] = (dest_port >> 0) & 0xFF;

    if (send_all(s, (const char*)request, sizeof(request)) != sizeof(request))
    {
        log_message("SOCKS5: Failed to send CONNECT");
        return -1;
    }

    int reply;
    if (socks5_read_connect_reply(s, &reply) != 0)
    {
        log_message("SOCKS5: CONNECT failed (reply=%d)", reply);
        return -1;
    }
    return 0;
}

int socks5_connect_v6(SOCKET s, const UINT8 dest_ip6[16], UINT16 dest_port)
{
    unsigned char request[22];
    if (socks5_negotiate_no_auth(s) != 0) return -1;

    request[0] = SOCKS5_VERSION;
    request[1] = SOCKS5_CMD_CONNECT;
    request[2] = 0x00;
    request[3] = SOCKS5_ATYP_IPV6;
    memcpy(&request[4], dest_ip6, 16);
    request[20] = (dest_port >> 8) & 0xFF;
    request[21] = (dest_port >> 0) & 0xFF;

    if (send_all(s, (const char*)request, sizeof(request)) != sizeof(request)) return -1;

    // The proxy may reply with any BND.ADDR type, not necessarily IPv6.
    int reply;
    if (socks5_read_connect_reply(s, &reply) != 0)
    {
        log_message("SOCKS5 IPv6: CONNECT failed (reply=%d)", reply);
        return -1;
    }
    return 0;
}

int socks5_udp_associate_with_config(SOCKET s, struct sockaddr_in *relay_addr)
{
    unsigned char request[10] = {
        SOCKS5_VERSION, SOCKS5_CMD_UDP_ASSOCIATE, 0x00, SOCKS5_ATYP_IPV4,
        0, 0, 0, 0, 0, 0
    };
    if (socks5_negotiate_no_auth(s) != 0) return -1;
    if (send_all(s, (const char*)request, sizeof(request)) != sizeof(request))
        return -1;

    // Reply: VER REP RSV ATYP BND.ADDR BND.PORT. The proxy picks the BND.ADDR type
    // independently (RFC 1928), and the reply can split across TCP segments.
    // The IPv4 UDP send socket requires an IPv4 bound endpoint.
    unsigned char rep[4];
    if (recv_n(s, (char*)rep, 4) != 4 || rep[0] != SOCKS5_VERSION || rep[1] != 0x00)
        return -1;
    if (rep[3] != SOCKS5_ATYP_IPV4)
        return -1;
    unsigned char ap[6];
    if (recv_n(s, (char*)ap, 6) != 6)
        return -1;

    relay_addr->sin_family = AF_INET;
    memcpy(&relay_addr->sin_addr.s_addr, ap, 4);
    memcpy(&relay_addr->sin_port, ap + 4, 2);
    return 0;
}

// connect UDP ASSOCIATE with SOCKS5 proxy (per proxy config)
BOOL establish_udp_associate_for_config(PROXY_CONFIG *cfg)
{
    if (cfg == NULL || cfg->host[0] == '\0' || cfg->port == 0)
        return FALSE;
    if (cfg->type != PROXY_TYPE_SOCKS5)
        return FALSE;

    // Prevent retry spam - only try every 1 second per config
    ULONGLONG now = GetTickCount64();
    if (now - cfg->last_udp_attempt < 1000)
    {
        log_message("[UDP ASSOC] Retry guard active for %s:%d, skipping", cfg->host, cfg->port);
        return FALSE;
    }

    cfg->last_udp_attempt = now;

    // Close existing connections if any
    if (cfg->udp_tcp_ctrl != INVALID_SOCKET)
    {
        closesocket(cfg->udp_tcp_ctrl);
        cfg->udp_tcp_ctrl = INVALID_SOCKET;
    }
    if (cfg->udp_send_sock != INVALID_SOCKET)
    {
        closesocket(cfg->udp_send_sock);
        cfg->udp_send_sock = INVALID_SOCKET;
    }

    // Create TCP control connection
    SOCKET tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock == INVALID_SOCKET)
        return FALSE;

    configure_tcp_socket(tcp_sock, 262144, 3000);

    UINT32 socks5_ip = resolve_hostname(cfg->host);
    if (socks5_ip == 0)
    {
        closesocket(tcp_sock);
        return FALSE;
    }

    struct sockaddr_in socks_addr;
    memset(&socks_addr, 0, sizeof(socks_addr));
    socks_addr.sin_family = AF_INET;
    socks_addr.sin_addr.s_addr = socks5_ip;
    socks_addr.sin_port = htons(cfg->port);

    // Bounded connect: a dead/unreachable proxy config fails in ~2s instead of stalling
    // the single-threaded relay for the full OS SYN timeout (~21s), which was delaying
    // real packets that use a *different*, working proxy config.
    if (connect_with_timeout(tcp_sock, (struct sockaddr *)&socks_addr, sizeof(socks_addr), 2000) == SOCKET_ERROR)
    {
        closesocket(tcp_sock);
        return FALSE;
    }

    if (socks5_udp_associate_with_config(tcp_sock, &cfg->udp_relay_addr) != 0)
    {
        closesocket(tcp_sock);
        return FALSE;
    }

    // Many SOCKS5 servers return 0.0.0.0 as BND.ADDR in
    // the UDP ASSOCIATE reply as per RFC 1928 says "use the same address
    // as the TCP control connection".  sendto(0.0.0.0:PORT) fails with
    // WSAEADDRNOTAVAIL (10049), so replace it with the proxy's resolved IP.
    if (cfg->udp_relay_addr.sin_addr.s_addr == INADDR_ANY)
        cfg->udp_relay_addr.sin_addr.s_addr = socks5_ip;

    // haandshake completed remove the 3second timeout so the control socket stays open indefinitely
    // keepalives below will detect actual disconnection.
    DWORD zero_timeout = 0;
    setsockopt(tcp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&zero_timeout, sizeof(zero_timeout));
    setsockopt(tcp_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&zero_timeout, sizeof(zero_timeout));

    // we can enable TCP keepalives so the SOCKS5 proxy dont idleclose the control
    // connection (few proxies terminate it after 60 second of silence, killing UDP ASSOCIATE).
    BOOL ka_on = TRUE;
    setsockopt(tcp_sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&ka_on, sizeof(ka_on));
    struct tcp_keepalive ka = { 1, 10000, 2000 }; // idle 10s, retry every 2s
    DWORD ka_bytes;
    WSAIoctl(tcp_sock, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), NULL, 0, &ka_bytes, NULL, NULL);

    cfg->udp_tcp_ctrl = tcp_sock;

    // create UDP socket for sending to SOCKS5 proxy
    cfg->udp_send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (cfg->udp_send_sock == INVALID_SOCKET)
    {
        closesocket(cfg->udp_tcp_ctrl);
        cfg->udp_tcp_ctrl = INVALID_SOCKET;
        cfg->udp_connected = FALSE;
        return FALSE;
    }

    configure_udp_socket(cfg->udp_send_sock, 262144, 30000);

    cfg->udp_connected = TRUE;
    log_message("UDP ASSOCIATE established with SOCKS5 proxy %s:%d (UDP relay at %s:%d)",
        cfg->host, cfg->port,
        inet_ntoa(cfg->udp_relay_addr.sin_addr), ntohs(cfg->udp_relay_addr.sin_port));
    return TRUE;
}
