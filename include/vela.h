#ifndef ORION_VELA_H
#define ORION_VELA_H

#include <stdint.h>
#include "vela_platform.h"
#include "aster.h"

#define VELA_NAME "UN_Vela"
#define VELA_VERSION "0.2.0-dev"
#define VELA_API_MAJOR 1u
#define VELA_API_MINOR 1u
#define VELA_API_VERSION ((VELA_API_MAJOR << 16) | VELA_API_MINOR)

#define VELA_CAP_PLATFORM_ABI (1ull << 0)
#define VELA_CAP_HISTORY      (1ull << 1)
#define VELA_CAP_SCROLL       (1ull << 2)
#define VELA_CAP_LOCAL_HTML   (1ull << 3)
#define VELA_CAP_ASTER_DOC    (1ull << 4)

void vela_init(int viewport_width);
int vela_init_ex(int viewport_width,const VelaPlatformOps *platform,void *platform_context);
int vela_set_platform(const VelaPlatformOps *platform,void *platform_context);
uint32_t vela_api_version(void);
uint64_t vela_capabilities(void);
uint64_t vela_platform_capabilities(void);

void vela_set_viewport(int viewport_width);
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

void vela_set_scroll(int scroll_y);
void vela_scroll_by(int delta_y);
int vela_scroll(void);

const char *vela_url(void);
const char *vela_status(void);
const char *vela_title(void);
const AsterDocument *vela_document(void);
void vela_paint(int x,int y,int width,int height);

#endif
