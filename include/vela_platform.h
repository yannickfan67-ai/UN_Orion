#ifndef UN_VELA_PLATFORM_H
#define UN_VELA_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#define VELA_PLATFORM_ABI_MAJOR 1u
#define VELA_PLATFORM_ABI_MINOR 1u
#define VELA_PLATFORM_ABI_VERSION ((VELA_PLATFORM_ABI_MAJOR << 16) | VELA_PLATFORM_ABI_MINOR)
#define VELA_PLATFORM_CAP_HTTP (1ull << 0)
#define VELA_PLATFORM_CAP_REPAINT (1ull << 1)
#define VELA_PLATFORM_CAP_LOG (1ull << 2)
#define VELA_PLATFORM_CAP_TIME (1ull << 3)
#define VELA_PLATFORM_CAP_OPEN_EXTERNAL (1ull << 4)
#define VELA_PLATFORM_CAP_CLIPBOARD (1ull << 5)
#define VELA_PLATFORM_CAP_TLS (1ull << 6)
#define VELA_PLATFORM_CAP_FILES (1ull << 7)
#define VELA_PLATFORM_CAP_RESOURCES (1ull << 8)
typedef struct VelaPlatformOps {
    uint32_t abi_version;uint32_t struct_size;uint64_t capabilities;
    int (*http_get)(void*,const char*,char*,size_t,char*,size_t);
    void (*request_repaint)(void*);void (*log)(void*,uint32_t,const char*);uint64_t (*time_ms)(void*);
    int (*open_external)(void*,const char*);int (*clipboard_set)(void*,const char*);int (*clipboard_get)(void*,char*,size_t);
    int (*resource_get)(void*,const char*,uint8_t*,size_t,size_t*,char*,size_t,char*,size_t);
    void *reserved[7];
} VelaPlatformOps;
static inline int vela_platform_is_compatible(const VelaPlatformOps*ops){if(!ops)return 0;if((ops->abi_version>>16)!=VELA_PLATFORM_ABI_MAJOR)return 0;return ops->struct_size>=offsetof(VelaPlatformOps,http_get)+sizeof(ops->http_get);}
#endif
