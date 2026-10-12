#include "../src/pb_relay.c"
#include "../src/pb_socks5.c"
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while (0)
static char last_log[1024];
static int log_count;
static void capture_log(const char *message) { log_count++; strncpy_s(last_log, sizeof(last_log), message, _TRUNCATE); }
int main(void)
{
    WSADATA wsa;
    CHECK(WSAStartup(MAKEWORD(2,2), &wsa) == 0);
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address = {0}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    CHECK(listen(listener, 1) == 0);
    int size = sizeof(address);
    CHECK(getsockname(listener, (struct sockaddr *)&address, &size) == 0);
    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(connect(client, (struct sockaddr *)&address, sizeof(address)) == 0);
    SOCKET peer = accept(listener, NULL, NULL);
    CHECK(peer != INVALID_SOCKET);
    DWORD timeout = 3000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    reset_close(peer);
    char buffer[8];
    CHECK(recv(client, buffer, sizeof(buffer), 0) == SOCKET_ERROR);
    CHECK(WSAGetLastError() == WSAECONNRESET);
    closesocket(client); closesocket(listener);
    ProxyBridge_SetLogCallback(capture_log);
    log_count = 0;
    socks5_log_connect_failure("IPv4", "203.0.113.5", 443, 5);
    CHECK(strstr(last_log, "203.0.113.5:443") && strstr(last_log, "connection refused"));
    socks5_log_connect_failure("IPv4", "203.0.113.5", 443, 5);
    CHECK(log_count == 1);
    socks5_log_connect_failure("IPv4", "203.0.113.6", 443, -1);
    CHECK(log_count == 3 && strstr(last_log, "did not answer"));
    SOCKET sender = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(socks5_connect(sender, inet_addr("203.0.113.5"), 443) != 0);
    CHECK(strstr(last_log, "negotiation failed") != NULL);
    char oversized[257]; memset(oversized, 'a', 256); oversized[256] = 0;
    CHECK(socks5_connect_domain(sender, oversized, 443) != 0);
    closesocket(sender);
    ProxyBridge_SetLogCallback(NULL);
    WSACleanup();
    puts("Handshake failure resets the application connection; invalid domains are rejected");
    return 0;
}
