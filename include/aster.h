#ifndef ORION_ASTER_H
#define ORION_ASTER_H
#include <stdint.h>
#include <stddef.h>
#define ASTER_VERSION "0.3.1"
#define ASTER_API_MAJOR 1u
#define ASTER_API_MINOR 3u
#define ASTER_API_VERSION ((ASTER_API_MAJOR << 16) | ASTER_API_MINOR)
#define ASTER_MAX_NODES 384
#define ASTER_MAX_PAINT 512
#define ASTER_TEXT_CAP 24576
#define ASTER_MAX_CSS_RULES 32
#define ASTER_PAINT_LINK 1u
#define ASTER_PAINT_BOLD 2u
#define ASTER_PAINT_IMAGE 4u
#define ASTER_PAINT_UNDERLINE 8u
#define ASTER_STYLE_HIDDEN 1u
#define ASTER_STYLE_BOLD 2u
#define ASTER_STYLE_UNDERLINE 4u
typedef enum {ASTER_NODE_ROOT=0,ASTER_NODE_ELEMENT,ASTER_NODE_TEXT} AsterNodeType;
typedef enum {ASTER_TAG_UNKNOWN=0,ASTER_TAG_HTML,ASTER_TAG_HEAD,ASTER_TAG_BODY,ASTER_TAG_TITLE,ASTER_TAG_H1,ASTER_TAG_H2,ASTER_TAG_H3,ASTER_TAG_P,ASTER_TAG_DIV,ASTER_TAG_BR,ASTER_TAG_HR,ASTER_TAG_A,ASTER_TAG_UL,ASTER_TAG_OL,ASTER_TAG_LI,ASTER_TAG_STRONG,ASTER_TAG_EM,ASTER_TAG_CODE,ASTER_TAG_SPAN,ASTER_TAG_HEADER,ASTER_TAG_FOOTER,ASTER_TAG_MAIN,ASTER_TAG_NAV,ASTER_TAG_SECTION,ASTER_TAG_ARTICLE,ASTER_TAG_BLOCKQUOTE,ASTER_TAG_SCRIPT,ASTER_TAG_STYLE,ASTER_TAG_IMG,ASTER_TAG_BUTTON} AsterTag;
typedef struct {uint8_t type,tag;int16_t parent,first_child,next_sibling;uint16_t text_off,text_len,href_off,href_len,src_off,src_len;uint16_t id_off,id_len,class_off,class_len;uint16_t width_hint,height_hint;uint8_t style_flags,style_scale;uint32_t style_color;} AsterNode;
typedef struct {uint8_t tag,selector_kind,flags,scale,color_set;uint16_t selector_off,selector_len;uint32_t color;} AsterCssRule;
typedef struct {int16_t x,y,w,h;uint8_t scale,flags;uint32_t color;uint16_t text_off,text_len,href_off,href_len,src_off,src_len;} AsterPaintItem;
typedef struct {AsterNode nodes[ASTER_MAX_NODES];AsterPaintItem paint[ASTER_MAX_PAINT];AsterCssRule css[ASTER_MAX_CSS_RULES];char text[ASTER_TEXT_CAP];uint16_t node_count,paint_count,text_used;uint8_t css_count;int document_height;char title[96];} AsterDocument;
void aster_document_init(AsterDocument *doc);int aster_parse_html(AsterDocument *doc,const char *html);void aster_layout(AsterDocument *doc,int viewport_width);void aster_paint(const AsterDocument *doc,int x,int y,int width,int height,int scroll_y);int aster_link_at(const AsterDocument *doc,int x,int y,int scroll_y,char *url,size_t url_cap);int aster_document_height(const AsterDocument *doc);uint32_t aster_api_version(void);const char *aster_version(void);
#endif
