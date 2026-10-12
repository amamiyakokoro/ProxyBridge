#include "../src/pb_internal.h"
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while (0)

int main(void)
{
    UINT32 source = inet_addr("192.0.2.1"), target = inet_addr("203.0.113.5");
    const UINT16 port = 51000;
    add_connection(port, TRUE, source, target, 443, 1);
    CHECK(udp_flow_validate(port, FALSE, source, NULL, target, NULL, 443));
    CHECK(!udp_flow_validate(port, FALSE, source, NULL, target, NULL, 53));
    CHECK(!is_connection_tracked(port, TRUE, FALSE));
    CHECK(connection_rev_table[rev_hash_v4(target, 443)] == NULL);
    add_connection(port, TRUE, source, target, 443, 1);
    connection_hash_table[port % CONNECTION_HASH_SIZE]->last_activity = GetTickCount64() - 20001;
    CHECK(!udp_flow_validate(port, FALSE, source, NULL, target, NULL, 443));
    add_connection(port, TRUE, source, target, 443, 1);
    ProxyBridge_SetProxyUdpDnsEnabled(TRUE);
    CHECK(!udp_flow_validate(port, FALSE, source, NULL, target, NULL, 443));
    udp_direct_remember(FALSE, &source, port, &target, 443, g_udp_generation);
    CHECK(udp_direct_cached(FALSE, &source, port, &target, 443));
    CHECK(!udp_direct_cached(FALSE, &source, port, &target, 53));
    UINT32 other_source = inet_addr("192.0.2.2");
    CHECK(!udp_direct_cached(FALSE, &other_source, port, &target, 443));
    LONG old_generation = g_udp_generation;
    invalidate_udp_decisions();
    CHECK(!udp_direct_cached(FALSE, &source, port, &target, 443));
    udp_direct_remember(FALSE, &source, port, &target, 443, old_generation);
    CHECK(!udp_direct_cached(FALSE, &source, port, &target, 443));
    UINT8 address6[16] = {0}; address6[15] = 5;
    add_connection_v6(port, TRUE, address6, address6, 443, 1);
    CHECK(udp_flow_validate(port, TRUE, 0, address6, 0, address6, 443));
    CHECK(!udp_flow_validate(port, TRUE, 0, address6, 0, address6, 53));
    cache_pid(source, port, 123, TRUE);
    cache_owner_event(address6, port, address6, 443, TRUE, 123, 77, WINDIVERT_LAYER_SOCKET);
    remove_owner_event_endpoint(77, WINDIVERT_LAYER_SOCKET);
    CHECK(get_cached_pid(source, port, TRUE) == 0);
    CHECK(sizeof(((PID_CACHE_ENTRY *)0)->timestamp) == 8);
    puts("UDP destination, idle, policy, owner and DIRECT cache checks passed");
    return 0;
}
