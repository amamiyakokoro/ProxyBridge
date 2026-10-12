#include "../src/pb_relay.c"

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; } } while (0)

int main(void)
{
    UINT32 target = inet_addr("203.0.113.5");
    CHECK(relay_peer_ok_v4(target, target));
    CHECK(relay_peer_ok_v4(inet_addr("127.0.0.2"), target));
    CHECK(!relay_peer_ok_v4(inet_addr("192.0.2.10"), target));
    UINT8 target6[16];
    struct in6_addr peer;
    CHECK(InetPtonA(AF_INET6, "2001:db8::5", target6) == 1);
    CHECK(InetPtonA(AF_INET6, "2001:db8::5", &peer) == 1);
    CHECK(relay_peer_ok_v6(&peer, target6));
    CHECK(InetPtonA(AF_INET6, "::1", &peer) == 1);
    CHECK(relay_peer_ok_v6(&peer, target6));
    CHECK(InetPtonA(AF_INET6, "::ffff:127.0.0.2", &peer) == 1);
    CHECK(relay_peer_ok_v6(&peer, target6));
    CHECK(InetPtonA(AF_INET6, "2001:db8::10", &peer) == 1);
    CHECK(!relay_peer_ok_v6(&peer, target6));
    puts("Relay accepts tracked destinations and rejects unrelated peers");
    return 0;
}
