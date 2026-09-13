#ifndef ORION_DHCP_H
#define ORION_DHCP_H

#include <stddef.h>
#include <stdint.h>

#define ORION_DHCP_DISCOVER 1u
#define ORION_DHCP_OFFER 2u
#define ORION_DHCP_REQUEST 3u
#define ORION_DHCP_ACK 5u
#define ORION_DHCP_NAK 6u

typedef struct {
    uint8_t message_type;
    uint8_t ip[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
    uint8_t dns[4];
    uint8_t server[4];
    uint32_t lease_seconds;
    uint8_t have_netmask;
    uint8_t have_gateway;
    uint8_t have_dns;
    uint8_t have_server;
} OrionDhcpLease;

size_t dhcp_build_discover(uint8_t *packet, size_t cap, uint32_t xid,
                           const uint8_t mac[6]);
size_t dhcp_build_request(uint8_t *packet, size_t cap, uint32_t xid,
                          const uint8_t mac[6], const uint8_t requested_ip[4],
                          const uint8_t server_id[4]);
int dhcp_parse_reply(const uint8_t *packet, size_t len, uint32_t xid,
                     const uint8_t mac[6], OrionDhcpLease *lease);

/*
 * Run one synchronous DHCPv4 DORA exchange over the already initialized
 * netdev. The existing static network profile is untouched on failure.
 */
int dhcp_acquire(const uint8_t mac[6], uint32_t timeout_ms,
                 OrionDhcpLease *lease);

#endif
