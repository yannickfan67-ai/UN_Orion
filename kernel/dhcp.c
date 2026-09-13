#include <stddef.h>
#include <stdint.h>
#include "dhcp.h"

#define DHCP_PACKET_MIN 300u
#define DHCP_COOKIE 0x63825363u

static void dcopy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}

static void dzero(void *dst, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = 0;
}

static int dequal(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    while (n--) if (*x++ != *y++) return 0;
    return 1;
}

static void dput16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void dput32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint16_t dget16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t dget32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static int ip_nonzero(const uint8_t ip[4]) {
    return ip[0] || ip[1] || ip[2] || ip[3];
}

static size_t dhcp_base(uint8_t *packet, size_t cap, uint32_t xid,
                        const uint8_t mac[6], uint8_t message_type) {
    if (!packet || !mac || cap < DHCP_PACKET_MIN) return 0;
    dzero(packet, DHCP_PACKET_MIN);
    packet[0] = 1; /* BOOTREQUEST */
    packet[1] = 1; /* Ethernet */
    packet[2] = 6;
    dput32(packet + 4, xid);
    dput16(packet + 10, 0x8000u); /* request broadcast replies */
    dcopy(packet + 28, mac, 6);
    dput32(packet + 236, DHCP_COOKIE);

    size_t o = 240;
    packet[o++] = 53; packet[o++] = 1; packet[o++] = message_type;
    packet[o++] = 61; packet[o++] = 7; packet[o++] = 1;
    dcopy(packet + o, mac, 6); o += 6;
    packet[o++] = 57; packet[o++] = 2; dput16(packet + o, 576); o += 2;
    return o;
}

static size_t dhcp_finish(uint8_t *packet, size_t cap, size_t o) {
    static const uint8_t requested[] = {1, 3, 6, 51};
    if (!packet || cap < DHCP_PACKET_MIN || o + 7 > DHCP_PACKET_MIN) return 0;
    packet[o++] = 55;
    packet[o++] = (uint8_t)sizeof(requested);
    dcopy(packet + o, requested, sizeof(requested));
    o += sizeof(requested);
    packet[o++] = 255;
    (void)o;
    return DHCP_PACKET_MIN;
}

size_t dhcp_build_discover(uint8_t *packet, size_t cap, uint32_t xid,
                           const uint8_t mac[6]) {
    size_t o = dhcp_base(packet, cap, xid, mac, ORION_DHCP_DISCOVER);
    return o ? dhcp_finish(packet, cap, o) : 0;
}

size_t dhcp_build_request(uint8_t *packet, size_t cap, uint32_t xid,
                          const uint8_t mac[6], const uint8_t requested_ip[4],
                          const uint8_t server_id[4]) {
    if (!requested_ip || !server_id) return 0;
    size_t o = dhcp_base(packet, cap, xid, mac, ORION_DHCP_REQUEST);
    if (!o || o + 12 > DHCP_PACKET_MIN) return 0;
    packet[o++] = 50; packet[o++] = 4; dcopy(packet + o, requested_ip, 4); o += 4;
    packet[o++] = 54; packet[o++] = 4; dcopy(packet + o, server_id, 4); o += 4;
    return dhcp_finish(packet, cap, o);
}

int dhcp_parse_reply(const uint8_t *packet, size_t len, uint32_t xid,
                     const uint8_t mac[6], OrionDhcpLease *lease) {
    if (!packet || !mac || !lease || len < 240) return 0;
    if (packet[0] != 2 || packet[1] != 1 || packet[2] != 6) return 0;
    if (dget32(packet + 4) != xid) return 0;
    if (!dequal(packet + 28, mac, 6)) return 0;
    if (dget32(packet + 236) != DHCP_COOKIE) return 0;

    dzero(lease, sizeof(*lease));
    dcopy(lease->ip, packet + 16, 4);

    size_t o = 240;
    while (o < len) {
        uint8_t code = packet[o++];
        if (code == 0) continue;
        if (code == 255) break;
        if (o >= len) return 0;
        uint8_t n = packet[o++];
        if ((size_t)n > len - o) return 0;
        const uint8_t *v = packet + o;

        if (code == 53 && n == 1) {
            lease->message_type = v[0];
        } else if (code == 1 && n == 4) {
            dcopy(lease->netmask, v, 4);
            lease->have_netmask = 1;
        } else if (code == 3 && n >= 4) {
            dcopy(lease->gateway, v, 4);
            lease->have_gateway = 1;
        } else if (code == 6 && n >= 4) {
            dcopy(lease->dns, v, 4);
            lease->have_dns = 1;
        } else if (code == 51 && n == 4) {
            lease->lease_seconds = dget32(v);
        } else if (code == 54 && n == 4) {
            dcopy(lease->server, v, 4);
            lease->have_server = 1;
        }
        o += n;
    }
    return lease->message_type != 0;
}

#ifndef ORION_DHCP_HOST_TEST

#include "interrupts.h"
#include "netdev.h"

#define ETH_IP 0x0800u
#define IP_UDP 17u
#define DHCP_CLIENT_PORT 68u
#define DHCP_SERVER_PORT 67u

static volatile int dhcp_phase;
static volatile int dhcp_event;
static uint32_t dhcp_xid;
static uint8_t dhcp_mac[6];
static OrionDhcpLease dhcp_rx_lease;
static uint16_t dhcp_ip_id = 0x4f52u;

static uint16_t dhcp_checksum16(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len) sum += (uint16_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint32_t dhcp_now_ms(void) {
    uint32_t hz = timer_frequency();
    if (!hz) return 0;
    return (uint32_t)((timer_ticks() * 1000ULL) / hz);
}

static int dhcp_elapsed(uint32_t start, uint32_t ms) {
    return (uint32_t)(dhcp_now_ms() - start) >= ms;
}

static int dhcp_send_broadcast(const uint8_t *payload, size_t payload_len) {
    if (!payload || payload_len > 1472u) return 0;
    uint8_t frame[1514];
    static const uint8_t broadcast_mac[6] = {255,255,255,255,255,255};
    static const uint8_t zero_ip[4] = {0,0,0,0};
    static const uint8_t broadcast_ip[4] = {255,255,255,255};

    dcopy(frame, broadcast_mac, 6);
    dcopy(frame + 6, dhcp_mac, 6);
    dput16(frame + 12, ETH_IP);

    uint8_t *ip = frame + 14;
    dzero(ip, 20);
    ip[0] = 0x45;
    dput16(ip + 2, (uint16_t)(20u + 8u + payload_len));
    dput16(ip + 4, dhcp_ip_id++);
    ip[8] = 64;
    ip[9] = IP_UDP;
    dcopy(ip + 12, zero_ip, 4);
    dcopy(ip + 16, broadcast_ip, 4);
    dput16(ip + 10, dhcp_checksum16(ip, 20));

    uint8_t *udp = ip + 20;
    dput16(udp, DHCP_CLIENT_PORT);
    dput16(udp + 2, DHCP_SERVER_PORT);
    dput16(udp + 4, (uint16_t)(8u + payload_len));
    dput16(udp + 6, 0); /* IPv4 permits a zero UDP checksum. */
    dcopy(udp + 8, payload, payload_len);
    return netdev_tx(frame, 14u + 20u + 8u + payload_len);
}

static void dhcp_rx(const uint8_t *frame, size_t len) {
    if (!frame || len < 14u + 20u + 8u) return;
    if (dget16(frame + 12) != ETH_IP) return;
    const uint8_t *ip = frame + 14;
    size_t ip_len = len - 14u;
    if ((ip[0] >> 4) != 4 || ip[9] != IP_UDP) return;
    size_t ihl = (size_t)(ip[0] & 15u) * 4u;
    if (ihl < 20u || ihl > ip_len) return;
    uint16_t total = dget16(ip + 2);
    if (total < ihl + 8u || total > ip_len) return;

    const uint8_t *udp = ip + ihl;
    size_t udp_avail = total - ihl;
    uint16_t udp_len = dget16(udp + 4);
    if (dget16(udp) != DHCP_SERVER_PORT || dget16(udp + 2) != DHCP_CLIENT_PORT)
        return;
    if (udp_len < 8u || udp_len > udp_avail) return;

    OrionDhcpLease reply;
    if (!dhcp_parse_reply(udp + 8, udp_len - 8u, dhcp_xid, dhcp_mac, &reply))
        return;
    if (reply.message_type == ORION_DHCP_NAK) {
        dhcp_event = -1;
        return;
    }
    if (dhcp_phase == 1 && reply.message_type == ORION_DHCP_OFFER) {
        dhcp_rx_lease = reply;
        dhcp_event = ORION_DHCP_OFFER;
    } else if (dhcp_phase == 2 && reply.message_type == ORION_DHCP_ACK) {
        dhcp_rx_lease = reply;
        dhcp_event = ORION_DHCP_ACK;
    }
}

static int dhcp_wait(const uint8_t *packet, size_t len, int phase,
                     int wanted, uint32_t timeout_ms) {
    dhcp_phase = phase;
    dhcp_event = 0;
    uint32_t start = dhcp_now_ms();
    uint32_t last_tx = 0xffffffffu;
    while (!dhcp_elapsed(start, timeout_ms)) {
        uint32_t now = dhcp_now_ms();
        if (last_tx == 0xffffffffu || (uint32_t)(now - last_tx) >= 400u) {
            dhcp_send_broadcast(packet, len);
            last_tx = now;
        }
        netdev_poll(dhcp_rx);
        if (dhcp_event == wanted) return 1;
        if (dhcp_event < 0) return 0;
        __asm__ volatile("pause");
    }
    return 0;
}

static void lease_overlay(OrionDhcpLease *dst, const OrionDhcpLease *src) {
    if (ip_nonzero(src->ip)) dcopy(dst->ip, src->ip, 4);
    if (src->have_netmask) {
        dcopy(dst->netmask, src->netmask, 4); dst->have_netmask = 1;
    }
    if (src->have_gateway) {
        dcopy(dst->gateway, src->gateway, 4); dst->have_gateway = 1;
    }
    if (src->have_dns) {
        dcopy(dst->dns, src->dns, 4); dst->have_dns = 1;
    }
    if (src->have_server) {
        dcopy(dst->server, src->server, 4); dst->have_server = 1;
    }
    if (src->lease_seconds) dst->lease_seconds = src->lease_seconds;
    dst->message_type = src->message_type;
}

int dhcp_acquire(const uint8_t mac[6], uint32_t timeout_ms,
                 OrionDhcpLease *lease) {
    if (!mac || !lease) return 0;
    if (timeout_ms < 1000u) timeout_ms = 1000u;
    uint32_t phase_timeout = timeout_ms / 2u;

    dcopy(dhcp_mac, mac, 6);
    dhcp_xid = 0x4f520000u ^ (uint32_t)timer_ticks();
    if (!dhcp_xid) dhcp_xid = 0x4f52494fu;

    uint8_t packet[DHCP_PACKET_MIN];
    size_t n = dhcp_build_discover(packet, sizeof(packet), dhcp_xid, dhcp_mac);
    if (!n || !dhcp_wait(packet, n, 1, ORION_DHCP_OFFER, phase_timeout)) {
        dhcp_phase = dhcp_event = 0;
        return 0;
    }

    OrionDhcpLease offer = dhcp_rx_lease;
    if (!ip_nonzero(offer.ip) || !offer.have_server) {
        dhcp_phase = dhcp_event = 0;
        return 0;
    }

    n = dhcp_build_request(packet, sizeof(packet), dhcp_xid, dhcp_mac,
                           offer.ip, offer.server);
    if (!n || !dhcp_wait(packet, n, 2, ORION_DHCP_ACK, phase_timeout)) {
        dhcp_phase = dhcp_event = 0;
        return 0;
    }

    OrionDhcpLease final = offer;
    lease_overlay(&final, &dhcp_rx_lease);
    if (!ip_nonzero(final.ip) || !final.have_netmask ||
        !final.have_gateway || !final.have_dns) {
        dhcp_phase = dhcp_event = 0;
        return 0;
    }
    *lease = final;
    dhcp_phase = dhcp_event = 0;
    return 1;
}

#else

int dhcp_acquire(const uint8_t mac[6], uint32_t timeout_ms,
                 OrionDhcpLease *lease) {
    (void)mac; (void)timeout_ms; (void)lease;
    return 0;
}

#endif
