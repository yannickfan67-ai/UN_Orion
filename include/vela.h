#ifndef ORION_VELA_H
#define ORION_VELA_H
#include <stddef.h>
#include <stdint.h>
#include "vela_platform.h"
#include "aster.h"
#define VELA_NAME "UN_Vela"
#define VELA_VERSION "0.3.1-dev"
#define VELA_API_MAJOR 1u
#define VELA_API_MINOR 3u
#define VELA_API_VERSION ((VELA_API_MAJOR << 16) | VELA_API_MINOR)
#define VELA_CAP_PLATFORM_ABI (1ull << 0)
#define VELA_CAP_HISTORY (1ull << 1)
#define VELA_CAP_SCROLL (1ull << 2)
#define VELA_CAP_LOCAL_HTML (1ull << 3)
#define VELA_CAP_ASTER_DOC (1ull << 4)
#define VELA_CAP_LINK_ACTIVATION (1ull << 5)
#define VELA_CAP_JS_SUBSET (1ull << 6)
#define VELA_CAP_FEATURE_PROFILE (1ull << 7)
#define VELA_CAP_RESOURCES (1ull << 8)
#define VELA_FEATURE_JAVASCRIPT (1u << 0)
#define VELA_PROFILE_LITE 0u
#define VELA_PROFILE_FULL VELA_FEATURE_JAVASCRIPT
void vela_init(int viewport_width);int vela_init_ex(int viewport_width,const VelaPlatformOps*,void*);int vela_set_platform(const VelaPlatformOps*,void*);uint32_t vela_api_version(void);uint64_t vela_capabilities(void);uint64_t vela_platform_capabilities(void);void vela_set_features(uint32_t);uint32_t vela_features(void);void vela_set_viewport(int);void vela_set_viewport_size(int,int);void vela_input_char(char);void vela_backspace(void);int vela_go(void);int vela_load_url(const char*);int vela_load_html(const char*,const char*);int vela_back(void);int vela_forward(void);int vela_reload(void);int vela_can_back(void);int vela_can_forward(void);int vela_activate_link(int,int);int vela_resource_get(const char*,uint8_t*,size_t,size_t*,char*,size_t,char*,size_t);void vela_set_scroll(int);void vela_scroll_by(int);int vela_scroll(void);const char*vela_url(void);const char*vela_status(void);const char*vela_title(void);const AsterDocument*vela_document(void);void vela_paint(int,int,int,int);
#endif
