#ifndef ORION_NETDEV_H
#define ORION_NETDEV_H
#include <stddef.h>
#include <stdint.h>
typedef void (*netdev_rx_fn)(const uint8_t *frame,size_t len);
int netdev_init(uint8_t mac_out[6]);
int netdev_tx(const void *frame,size_t len);
void netdev_poll(netdev_rx_fn rx);
const char *netdev_name(void);
#endif
