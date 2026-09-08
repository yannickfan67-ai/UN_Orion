#ifndef UN_VELA_PLATFORM_H
#define UN_VELA_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#define VELA_PLATFORM_ABI_MAJOR 1u
#define VELA_PLATFORM_ABI_MINOR 0u
#define VELA_PLATFORM_ABI_VERSION ((VELA_PLATFORM_ABI_MAJOR << 16) | VELA_PLATFORM_ABI_MINOR)

#define VELA_PLATFORM_CAP_HTTP          (1ull << 0)
#define VELA_PLATFORM_CAP_REPAINT       (1ull << 1)
#define VELA_PLATFORM_CAP_LOG           (1ull << 2)
#define VELA_PLATFORM_CAP_TIME          (1ull << 3)
#define VELA_PLATFORM_CAP_OPEN_EXTERNAL (1ull << 4)
#define VELA_PLATFORM_CAP_CLIPBOARD     (1ull << 5)
#define VELA_PLATFORM_CAP_TLS           (1ull << 6)
#define VELA_PLATFORM_CAP_FILES         (1ull << 7)

/* Stable append-only host boundary for UN_Vela ABI major 1. */
typedef struct VelaPlatformOps {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t capabilities;

    int (*http_get)(void *context,
                    const char *url,
                    char *body,
                    size_t body_cap,
                    char *status,
                    size_t status_cap);
    void (*request_repaint)(void *context);
    void (*log)(void *context, uint32_t level, const char *message);
    uint64_t (*time_ms)(void *context);
    int (*open_external)(void *context, const char *url);
    int (*clipboard_set)(void *context, const char *text);
    int (*clipboard_get)(void *context, char *text, size_t text_cap);

    void *reserved[8];
} VelaPlatformOps;

static inline int vela_platform_is_compatible(const VelaPlatformOps *ops) {
    if (!ops) return 0;
    if ((ops->abi_version >> 16) != VELA_PLATFORM_ABI_MAJOR) return 0;
    return ops->struct_size >= offsetof(VelaPlatformOps, http_get) + sizeof(ops->http_get);
}

#endif
