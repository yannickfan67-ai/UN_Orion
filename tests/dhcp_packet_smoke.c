#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dhcp.h"

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int find_option(const uint8_t *p, size_t len, uint8_t wanted,
                       const uint8_t **value, uint8_t *value_len) {
    size_t o = 240;
    while (o < len) {
        uint8_t code = p[o++];
        if (code == 0) continue;
        if (code == 255) return 0;
        if (o >= len) return 0;
        uint8_t n = p[o++];
        if ((size_t)n > len - o) return 0;
        if (code == wanted) {
            *value = p + o;
            *value_len = n;
            return 1;
        }
        o += n;
    }
    return 0;
}

static int eq4(const uint8_t a[4], const uint8_t b[4]) {
    return memcmp(a, b, 4) == 0;
}

int main(void) {
    const uint8_t mac[6] = {0x52,0x54,0x00,0x12,0x34,0x56};
    const uint8_t ip[4] = {10,0,2,15};
    const uint8_t server[4] = {10,0,2,2};
    const uint8_t mask[4] = {255,255,255,0};
    const uint8_t dns[4] = {10,0,2,3};
    const uint32_t xid = 0x1234abcd;
    uint8_t packet[300];

    size_t n = dhcp_build_discover(packet, sizeof(packet), xid, mac);
    if (n != sizeof(packet) || packet[0] != 1 || packet[1] != 1 ||
        packet[2] != 6 || memcmp(packet + 28, mac, 6) != 0) {
        fprintf(stderr, "discover BOOTP header invalid\n");
        return 1;
    }
    const uint8_t *v = NULL;
    uint8_t vn = 0;
    if (!find_option(packet, n, 53, &v, &vn) || vn != 1 ||
        v[0] != ORION_DHCP_DISCOVER) {
        fprintf(stderr, "discover message type missing\n");
        return 1;
    }

    n = dhcp_build_request(packet, sizeof(packet), xid, mac, ip, server);
    if (n != sizeof(packet)) return 1;
    if (!find_option(packet, n, 53, &v, &vn) || vn != 1 ||
        v[0] != ORION_DHCP_REQUEST) return 1;
    if (!find_option(packet, n, 50, &v, &vn) || vn != 4 ||
        memcmp(v, ip, 4) != 0) return 1;
    if (!find_option(packet, n, 54, &v, &vn) || vn != 4 ||
        memcmp(v, server, 4) != 0) return 1;

    memset(packet, 0, sizeof(packet));
    packet[0] = 2;
    packet[1] = 1;
    packet[2] = 6;
    put32(packet + 4, xid);
    memcpy(packet + 16, ip, 4);
    memcpy(packet + 28, mac, 6);
    put32(packet + 236, 0x63825363u);
    size_t o = 240;
    packet[o++] = 53; packet[o++] = 1; packet[o++] = ORION_DHCP_ACK;
    packet[o++] = 1; packet[o++] = 4; memcpy(packet + o, mask, 4); o += 4;
    packet[o++] = 3; packet[o++] = 4; memcpy(packet + o, server, 4); o += 4;
    packet[o++] = 6; packet[o++] = 4; memcpy(packet + o, dns, 4); o += 4;
    packet[o++] = 54; packet[o++] = 4; memcpy(packet + o, server, 4); o += 4;
    packet[o++] = 51; packet[o++] = 4; put32(packet + o, 3600); o += 4;
    packet[o++] = 255;

    OrionDhcpLease lease;
    if (!dhcp_parse_reply(packet, o, xid, mac, &lease)) {
        fprintf(stderr, "valid ACK rejected\n");
        return 1;
    }
    if (lease.message_type != ORION_DHCP_ACK || !eq4(lease.ip, ip) ||
        !lease.have_netmask || !eq4(lease.netmask, mask) ||
        !lease.have_gateway || !eq4(lease.gateway, server) ||
        !lease.have_dns || !eq4(lease.dns, dns) ||
        !lease.have_server || !eq4(lease.server, server) ||
        lease.lease_seconds != 3600u) {
        fprintf(stderr, "parsed lease mismatch\n");
        return 1;
    }

    if (dhcp_parse_reply(packet, o, xid ^ 1u, mac, &lease)) {
        fprintf(stderr, "wrong xid accepted\n");
        return 1;
    }
    packet[o - 2] = 54;
    packet[o - 1] = 8;
    if (dhcp_parse_reply(packet, o, xid, mac, &lease)) {
        fprintf(stderr, "truncated option accepted\n");
        return 1;
    }

    puts("DHCP packet smoke test passed");
    return 0;
}
