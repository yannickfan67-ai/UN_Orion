#include <stdint.h>
#include "serial.h"
#include "netdev.h"
#include "netdev_internal.h"
static const NetDriver *active;
static const char *active_name(void){return active?(active->get_name?active->get_name():active->name):"none";}
static const NetDriver *const drivers[]={&rtl8139_driver,&pcnet_driver,&e1000_driver,&virtio_net_driver};
int netdev_init(uint8_t mac[6]){
    active=0;
    for(unsigned i=0;i<sizeof(drivers)/sizeof(drivers[0]);i++)if(drivers[i]->probe_init(mac)){active=drivers[i];serial_write("NETDEV: ");serial_write(active_name());serial_write(" selected\r\n");return 1;}
    serial_write("NETDEV: no supported adapter found\r\n");return 0;
}
int netdev_tx(const void *p,size_t n){return active&&active->tx?active->tx(p,n):0;}
void netdev_poll(netdev_rx_fn f){if(active&&active->poll)active->poll(f);}
const char *netdev_name(void){return active_name();}
