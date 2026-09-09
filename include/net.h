#ifndef ORION_NET_H
#define ORION_NET_H
#include <stdint.h>
#include <stddef.h>
int net_init(void);void net_poll(void);int net_ready(void);const char *net_driver_name(void);void net_get_mac(uint8_t out[6]);void net_get_ipv4(uint8_t out[4]);void net_get_gateway(uint8_t out[4]);void net_configure(const uint8_t ip[4],const uint8_t mask[4],const uint8_t gw[4],const uint8_t dns[4]);uint64_t net_rx_packets(void);uint64_t net_tx_packets(void);int net_ping_gateway(uint32_t timeout_ms);int net_http_get(const char *url,char *body,size_t cap,char *status,size_t status_cap);int net_http_get_bytes(const char *url,uint8_t *data,size_t cap,size_t *len,char *status,size_t status_cap);
#endif
