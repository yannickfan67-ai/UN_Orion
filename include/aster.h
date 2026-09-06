#ifndef ORION_ASTER_H
#define ORION_ASTER_H

#include <stdint.h>
#include <stddef.h>

#define ASTER_VERSION "0.1.0"
#define ASTER_MAX_NODES 192
#define ASTER_MAX_PAINT 192
#define ASTER_TEXT_CAP 8192

typedef enum {
    ASTER_NODE_ROOT=0,
    ASTER_NODE_ELEMENT,
    ASTER_NODE_TEXT
} AsterNodeType;

typedef enum {
    ASTER_TAG_UNKNOWN=0,
    ASTER_TAG_HTML, ASTER_TAG_HEAD, ASTER_TAG_BODY,
    ASTER_TAG_TITLE, ASTER_TAG_H1, ASTER_TAG_H2,
    ASTER_TAG_P, ASTER_TAG_DIV, ASTER_TAG_BR,
    ASTER_TAG_A, ASTER_TAG_UL, ASTER_TAG_OL, ASTER_TAG_LI,
    ASTER_TAG_STRONG, ASTER_TAG_EM, ASTER_TAG_CODE
} AsterTag;

typedef struct {
    uint8_t type;
    uint8_t tag;
    int16_t parent;
    int16_t first_child;
    int16_t next_sibling;
    uint16_t text_off;
    uint16_t text_len;
    uint16_t href_off;
    uint16_t href_len;
} AsterNode;

typedef struct {
    int16_t x,y,w,h;
    uint8_t scale;
    uint8_t flags;
    uint32_t color;
    uint16_t text_off;
    uint16_t text_len;
    uint16_t href_off;
    uint16_t href_len;
} AsterPaintItem;

typedef struct {
    AsterNode nodes[ASTER_MAX_NODES];
    AsterPaintItem paint[ASTER_MAX_PAINT];
    char text[ASTER_TEXT_CAP];
    uint16_t node_count;
    uint16_t paint_count;
    uint16_t text_used;
    int document_height;
    char title[96];
} AsterDocument;

void aster_document_init(AsterDocument *doc);
int aster_parse_html(AsterDocument *doc,const char *html);
void aster_layout(AsterDocument *doc,int viewport_width);
void aster_paint(const AsterDocument *doc,int x,int y,int width,int height,int scroll_y);
const char *aster_version(void);

#endif
