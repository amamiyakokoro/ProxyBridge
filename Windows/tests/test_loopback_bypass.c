#include "../src/pb_internal.h"

static unsigned owner_lookups = 0;
static DWORD test_owner_event(const UINT8 *local, UINT16 local_port,
                              const UINT8 *remote, UINT16 remote_port, BOOL udp)
{
    owner_lookups++;
    return 0;
}
static DWORD test_owner_v4(UINT32 ip, UINT16 port)
{
    owner_lookups++;
    return 0;
}
static DWORD test_owner_v6(const UINT8 *ip, UINT16 port)
{
    owner_lookups++;
    return 0;
}

// Exercise the real rule entry points with an owner that never resolves. The test
// needs no packet capture, installed driver, elevated token, or existing sockets.
#define get_owner_event_pid test_owner_event
#define get_process_id_from_connection test_owner_v4
#define get_process_id_from_udp_connection test_owner_v4
#define get_process_id_from_connection_v6 test_owner_v6
#define get_process_id_from_udp_connection_v6 test_owner_v6
#include "../src/pb_rules.c"

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; \
} } while (0)

static RuleAction check_v4(const char *source, const char *destination, BOOL udp)
{
    DWORD pid = 99;
    UINT32 proxy_id = 99;
    RuleAction action = check_process_rule(inet_addr(source), 5577,
        inet_addr(destination), 64836, udp, &pid, &proxy_id);
    if (pid != 0 || proxy_id != 0) return RULE_ACTION_PROXY;
    return action;
}

static RuleAction check_v6(const char *source, const char *destination, BOOL udp)
{
    UINT8 src[16], dst[16];
    DWORD pid = 99;
    UINT32 proxy_id = 99;
    if (InetPtonA(AF_INET6, source, src) != 1 || InetPtonA(AF_INET6, destination, dst) != 1)
        return RULE_ACTION_PROXY;
    RuleAction action = check_process_rule_v6(src, 5577, dst, 64836, udp, &pid, &proxy_id);
    if (pid != 0 || proxy_id != 0) return RULE_ACTION_PROXY;
    return action;
}

int main(void)
{
    WSADATA winsock;
    CHECK(WSAStartup(MAKEWORD(2, 2), &winsock) == 0);
    ProxyBridge_SetFailClosedOnUnknownOwner(TRUE);
    UINT32 rule_id = ProxyBridge_AddRule("*", "*", "*", "*", RULE_PROTOCOL_BOTH, RULE_ACTION_BLOCK, 0);
    CHECK(rule_id != 0);

    // The new policy is opt-in: existing callers retain their BLOCK behavior.
    CHECK(check_v4("127.0.0.1", "127.0.0.1", FALSE) == RULE_ACTION_BLOCK);
    CHECK(owner_lookups > 0);
    ProxyBridge_SetLoopbackBypassEnabled(TRUE);
    owner_lookups = 0;
    for (int i = 0; i < 64; i++) {
        CHECK(check_v4("127.0.0.1", "127.0.0.1", FALSE) == RULE_ACTION_DIRECT);
        CHECK(check_v4("127.255.0.1", "127.0.0.2", TRUE) == RULE_ACTION_DIRECT);
        CHECK(check_v6("::1", "::1", FALSE) == RULE_ACTION_DIRECT);
        CHECK(check_v6("::ffff:127.0.0.1", "::ffff:127.0.0.2", TRUE) == RULE_ACTION_DIRECT);
    }
    CHECK(owner_lookups == 0);

    // Either endpoint outside loopback must still take the fail-closed path.
    CHECK(check_v4("127.0.0.1", "203.0.113.1", FALSE) == RULE_ACTION_BLOCK);
    CHECK(check_v4("203.0.113.1", "127.0.0.1", TRUE) == RULE_ACTION_BLOCK);
    CHECK(check_v6("::1", "2001:db8::1", FALSE) == RULE_ACTION_BLOCK);
    CHECK(check_v6("2001:db8::1", "::1", TRUE) == RULE_ACTION_BLOCK);
    CHECK(check_v6("::ffff:192.0.2.1", "::ffff:127.0.0.1", FALSE) == RULE_ACTION_BLOCK);
    CHECK(owner_lookups > 0);

    // Runtime toggles must discard old source-port decisions.
    InterlockedExchange(&port_decided_bitmap[1], 1);
    InterlockedExchange(&port_direct_bitmap[1], 1);
    ProxyBridge_SetLoopbackBypassEnabled(FALSE);
    CHECK(port_decided_bitmap[1] == 0 && port_direct_bitmap[1] == 0);
    CHECK(check_v6("::1", "::1", FALSE) == RULE_ACTION_BLOCK);
    CHECK(ProxyBridge_DeleteRule(rule_id));
    WSACleanup();
    puts("PASS: loopback bypass avoids owner lookup; external traffic remains fail closed");
    return 0;
}
