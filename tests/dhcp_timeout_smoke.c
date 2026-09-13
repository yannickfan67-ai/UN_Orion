#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dhcp.h"
#include "netdev.h"

static uint64_t fake_ticks;
static unsigned tx_count;
static unsigned poll_count;

uint64_t timer_ticks(void) { return fake_ticks; }
uint32_t timer_frequency(void) { return 1000; }

int netdev_tx(const void *frame, size_t len) {
    if (!frame || len < 300) return 0;
    tx_count++;
    return 1;
}

void netdev_poll(netdev_rx_fn rx) {
    (void)rx;
    poll_count++;
    fake_ticks += 25;
}

int netdev_init(uint8_t mac_out[6]) { (void)mac_out; return 0; }
const char *netdev_name(void) { return "test"; }

int main(void) {
    const uint8_t mac[6] = {0x52,0x54,0x00,0xaa,0xbb,0xcc};
    OrionDhcpLease lease;
    memset(&lease, 0xa5, sizeof(lease));
    OrionDhcpLease before = lease;

    if (dhcp_acquire(mac, 1000, &lease)) {
        fprintf(stderr, "DHCP unexpectedly succeeded without a server\n");
        return 1;
    }
    if (memcmp(&lease, &before, sizeof(lease)) != 0) {
        fprintf(stderr, "failed DHCP attempt modified caller lease\n");
        return 1;
    }
    if (fake_ticks < 500 || fake_ticks > 750) {
        fprintf(stderr, "DHCP fallback was not bounded as expected: ticks=%llu\n",
                (unsigned long long)fake_ticks);
        return 1;
    }
    if (tx_count < 2 || poll_count < 2) {
        fprintf(stderr, "DHCP retry loop did not exercise transmission/polling\n");
        return 1;
    }

    puts("DHCP timeout/static-fallback smoke test passed");
    return 0;
}
