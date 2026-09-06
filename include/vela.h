#ifndef ORION_VELA_H
#define ORION_VELA_H

#define VELA_NAME "UN_Vela"
#define VELA_VERSION "0.1.0"

void vela_init(int viewport_width);
void vela_set_viewport(int viewport_width);
void vela_input_char(char c);
void vela_backspace(void);
int vela_go(void);
int vela_load_url(const char *url);
const char *vela_url(void);
const char *vela_status(void);
const char *vela_title(void);
void vela_paint(int x,int y,int width,int height);

#endif
