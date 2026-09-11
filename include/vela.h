#ifndef ORION_VELA_H
#define ORION_VELA_H
#include <stddef.h>
#include <stdint.h>
#include "vela_platform.h"
#include "aster.h"

#define VELA_NAME "UN_Vela"
#define VELA_VERSION "0.3.2-dev"
#define VELA_API_MAJOR 1u
#define VELA_API_MINOR 4u
#define VELA_API_VERSION ((VELA_API_MAJOR << 16) | VELA_API_MINOR)
#define VELA_CAP_PLATFORM_ABI    (1ull << 0)
#define VELA_CAP_HISTORY         (1ull << 1)
#define VELA_CAP_SCROLL          (1ull << 2)
#define VELA_CAP_LOCAL_HTML      (1ull << 3)
#define VELA_CAP_ASTER_DOC       (1ull << 4)
#define VELA_CAP_LINK_ACTIVATION (1ull << 5)
#define VELA_CAP_JS_SUBSET       (1ull << 6)
#define VELA_CAP_FEATURE_PROFILE (1ull << 7)
#define VELA_CAP_RESOURCES       (1ull << 8)
#define VELA_CAP_NAV_ACTIONS     (1ull << 9)
#define VELA_FEATURE_JAVASCRIPT  (1u << 0)
#define VELA_PROFILE_LITE 0u
#define VELA_PROFILE_FULL VELA_FEATURE_JAVASCRIPT

typedef enum VelaNavAction {
    VELA_NAV_BACK=1,
    VELA_NAV_FORWARD,
    VELA_NAV_RELOAD,
    VELA_NAV_LINE_UP,
    VELA_NAV_LINE_DOWN,
    VELA_NAV_PAGE_UP,
    VELA_NAV_PAGE_DOWN,
    VELA_NAV_HOME,
    VELA_NAV_END
} VelaNavAction;

void vela_init(int viewport_width);
int vela_init_ex(int viewport_width,const VelaPlatformOps *platform,void *platform_context);
int vela_set_platform(const VelaPlatformOps *platform,void *platform_context);
uint32_t vela_api_version(void);
uint64_t vela_capabilities(void);
uint64_t vela_platform_capabilities(void);
void vela_set_features(uint32_t features);
uint32_t vela_features(void);
void vela_set_viewport(int viewport_width);
void vela_set_viewport_size(int viewport_width,int viewport_height);
void vela_input_char(char c);
void vela_backspace(void);
int vela_go(void);
int vela_load_url(const char *url);
int vela_load_html(const char *html,const char *virtual_url);
int vela_back(void);
int vela_forward(void);
int vela_reload(void);
int vela_can_back(void);
int vela_can_forward(void);
int vela_navigate_action(VelaNavAction action);
int vela_activate_link(int x,int y);
int vela_resource_get(const char *ref,uint8_t *data,size_t data_cap,size_t *data_len,char *content_type,size_t content_type_cap,char *status,size_t status_cap);
void vela_set_scroll(int scroll_y);
void vela_scroll_by(int delta_y);
int vela_scroll(void);
const char *vela_url(void);
const char *vela_status(void);
const char *vela_title(void);
const AsterDocument *vela_document(void);
void vela_paint(int x,int y,int width,int height);
#endif
