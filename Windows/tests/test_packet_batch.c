// White-box test: include the core translation unit so the private batch
// compaction helper can be verified without installing/capturing with WinDivert.
#include "../src/ProxyBridge.c"

#include <stdio.h>

int main(void)
{
    UINT8 output[7] = {0};
    const UINT8 first[] = {1, 2};
    const UINT8 dropped[] = {3};
    const UINT8 last[] = {4, 5, 6};
    const UINT8 expected[] = {1, 2, 4, 5, 6};
    PACKET_SEND_BATCH batch;
    WINDIVERT_ADDRESS address1;
    WINDIVERT_ADDRESS address2;

    memset(&batch, 0, sizeof(batch));
    memset(&address1, 0, sizeof(address1));
    memset(&address2, 0, sizeof(address2));
    address1.Outbound = 1;
    address2.Loopback = 1;
    batch.packets = output;
    batch.capacity = sizeof(output);

    if (!queue_packet_for_send(&batch, first, sizeof(first), &address1))
        return 1;
    (void)dropped; // A dropped packet is deliberately not queued.
    if (!queue_packet_for_send(&batch, last, sizeof(last), &address2))
        return 1;

    if (batch.count != 2 || batch.length != sizeof(expected) ||
        memcmp(output, expected, sizeof(expected)) != 0 ||
        !batch.addresses[0].Outbound || !batch.addresses[1].Loopback)
        return 1;

    // A rejected append must not corrupt the already-packed batch.
    UINT old_count = batch.count;
    UINT old_length = batch.length;
    if (queue_packet_for_send(&batch, last, sizeof(last), &address2) ||
        batch.count != old_count || batch.length != old_length ||
        memcmp(output, expected, sizeof(expected)) != 0)
        return 1;

    puts("Packet batch compaction test passed");
    return 0;
}
