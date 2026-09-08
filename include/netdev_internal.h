#ifndef ORION_NETDEV_INTERNAL_H
#define ORION_NETDEV_INTERNAL_H
#include <stddef.h>
#include <stdint.h>
#include "netdev.h"
typedef struct {
    const char *name;
    const char *(*get_name)(void);
    int (*probe_init)(uint8_t mac[6]);
    int (*tx)(const void *,size_t);
    void (*poll)(netdev_rx_fn);
} NetDriver;
extern const NetDriver rtl8139_driver;
extern const NetDriver pcnet_driver;
extern const NetDriver e1000_driver;
extern const NetDriver virtio_net_driver;
#endif
