#ifndef ORION_TLS_H
#define ORION_TLS_H
#include <stddef.h>
#include <stdint.h>
int tls_https_exchange(const char *host,uint16_t port,const uint8_t *request,size_t request_len,uint8_t *response,size_t response_cap,size_t *response_len,char *status,size_t status_cap);
int tls_platform_ready(char *status,size_t status_cap);
#endif
