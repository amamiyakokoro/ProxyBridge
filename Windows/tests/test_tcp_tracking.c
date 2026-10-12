#include "../src/ProxyBridge.c"
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while (0)

int main(void)
{
    const UINT16 port = 51000;
    UINT32 destination;
    UINT16 destination_port;
    add_connection(port, FALSE, inet_addr("192.0.2.1"), inet_addr("203.0.113.5"), 443, 1);
    connection_fin(port, FALSE, 1);
    CHECK(get_connection(port, FALSE, &destination, &destination_port));
    CHECK(destination_port == 443);
    connection_fin(port, FALSE, 2);
    CHECK(is_connection_tracked(port, FALSE, FALSE)); // final ACK still needs translation
    CONNECTION_INFO *c = connection_hash_table[port % CONNECTION_HASH_SIZE];
    CHECK(c->fin_flags == 3);
    c->last_activity = GetTickCount64() - 60001;
    cleanup_stale_connections();
    CHECK(!is_connection_tracked(port, FALSE, FALSE));

    add_connection(port, FALSE, 0, inet_addr("203.0.113.5"), 443, 1);
    port_set_direct(port);
    reset_stale_tcp_state(port, FALSE);
    CHECK(!port_is_decided(port) && !is_connection_tracked(port, FALSE, FALSE));
    port_set_direct(port);
    CHECK(consume_direct_decision(port, TRUE));
    CHECK(!port_is_decided(port));

    UINT8 address6[16] = {0}; address6[15] = 5;
    add_connection_v6(port, FALSE, address6, address6, 443, 1);
    connection_fin(port, TRUE, 1);
    CHECK(is_connection_tracked(port, FALSE, TRUE));
    reset_stale_tcp_state(port, TRUE);
    CHECK(!is_connection_tracked(port, FALSE, TRUE));
    g_proxy_config_count = 1;
    g_proxy_configs[0].config_id = 1;
    g_proxy_configs[0].resolved_ip = inet_addr("127.0.0.1");
    g_proxy_configs[0].port = 7891;
    CHECK(is_proxy_server_endpoint(inet_addr("127.0.0.1"), 7891));
    CHECK(!is_proxy_server_endpoint(inet_addr("127.0.0.1"), 443));
    puts("TCP FIN grace, SYN reuse, DIRECT eviction and proxy endpoint checks passed");
    return 0;
}
