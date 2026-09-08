#ifndef ORION_ASTER_H
#define ORION_ASTER_H

#include <stdint.h>
#include <stddef.h>

#define ASTER_VERSION "0.2.0-dev"
#define ASTER_API_MAJOR 1u
#define ASTER_API_MINOR 1u
#define ASTER_API_VERSION ((ASTER_API_MAJOR << 16) | ASTER_API_MINOR)
#define ASTER_MAX_NODES 320
#define ASTER_MAX_PAINT 384
#define ASTER_TEXT_CAP 16384

#define ASTER_PAINT_LINK 1u
#define ASTER_PAINT_BOLD 2u

typedef enum {
    ASTER_NODE_ROOT=0,
    ASTER_NODE_ELEMENT,
    ASTER_NODE_TEXT
} AsterNodeType;

typedef enum {
    ASTER_TAG_UNKNOWN=0,
    ASTER_TAG_HTML, ASTER_TAG_HEAD, ASTER_TAG_BODY,
    ASTER_TAG_TITLE, ASTER_TAG_H1, ASTER_TAG_H2, ASTER_TAG_H3,
    ASTER_TAG_P, ASTER_TAG_DIV, ASTER_TAG_BR, ASTER_TAG_HR,
    ASTER_TAG_A, ASTER_TAG_UL, ASTER_TAG_OL, ASTER_TAG_LI,
    ASTER_TAG_STRONG, ASTER_TAG_EM, ASTER_TAG_CODE, ASTER_TAG_SPAN,
    ASTER_TAG_HEADER, ASTER_TAG_FOOTER, ASTER_TAG_MAIN, ASTER_TAG_NAV,
    ASTER_TAG_SECTION, ASTER_TAG_ARTICLE, ASTER_TAG_BLOCKQUOTE,
    ASTER_TAG_SCRIPT, ASTER_TAG_STYLE
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
int aster_link_at(const AsterDocument *doc,int x,int y,int scroll_y,char *url,size_t url_cap);
int aster_document_height(const AsterDocument *doc);
uint32_t aster_api_version(void);
const char *aster_version(void);

#endif
