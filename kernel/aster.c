#include <stdint.h>
#include <stddef.h>
#include "aster.h"
#include "graphics.h"

#define ASTER_FLAG_LINK 1
#define ASTER_FLAG_BOLD 2

static int is_space(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static char lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int starts_ci(const char *s,const char *p){while(*p){if(lower(*s++)!=lower(*p++))return 0;}return 1;}
static int name_eq(const char *s,size_t n,const char *lit){size_t i=0;while(lit[i]&&i<n){if(lower(s[i])!=lower(lit[i]))return 0;i++;}return i==n&&lit[i]==0;}

static uint16_t store_bytes(AsterDocument *d,const char *s,size_t n){
    if(!d||!s||!n||d->text_used>=ASTER_TEXT_CAP-1)return 0;
    if(n>ASTER_TEXT_CAP-1-d->text_used)n=ASTER_TEXT_CAP-1-d->text_used;
    uint16_t off=d->text_used;
    for(size_t i=0;i<n;i++)d->text[d->text_used++]=s[i];
    d->text[d->text_used]=0;
    return off;
}
static int add_node(AsterDocument*d,int parent,uint8_t type,uint8_t tag,uint16_t off,uint16_t len){
    if(d->node_count>=ASTER_MAX_NODES)return -1;
    int idx=d->node_count++;
    AsterNode*n=&d->nodes[idx];
    n->type=type;n->tag=tag;n->parent=(int16_t)parent;n->first_child=-1;n->next_sibling=-1;
    n->text_off=off;n->text_len=len;n->href_off=0;n->href_len=0;
    if(parent>=0){AsterNode*p=&d->nodes[parent];if(p->first_child<0)p->first_child=(int16_t)idx;else{int c=p->first_child;while(d->nodes[c].next_sibling>=0)c=d->nodes[c].next_sibling;d->nodes[c].next_sibling=(int16_t)idx;}}
    return idx;
}
static uint8_t tag_id(const char*s,size_t n){
    if(name_eq(s,n,"html"))return ASTER_TAG_HTML;if(name_eq(s,n,"head"))return ASTER_TAG_HEAD;if(name_eq(s,n,"body"))return ASTER_TAG_BODY;
    if(name_eq(s,n,"title"))return ASTER_TAG_TITLE;if(name_eq(s,n,"h1"))return ASTER_TAG_H1;if(name_eq(s,n,"h2"))return ASTER_TAG_H2;
    if(name_eq(s,n,"p"))return ASTER_TAG_P;if(name_eq(s,n,"div"))return ASTER_TAG_DIV;if(name_eq(s,n,"br"))return ASTER_TAG_BR;
    if(name_eq(s,n,"a"))return ASTER_TAG_A;if(name_eq(s,n,"ul"))return ASTER_TAG_UL;if(name_eq(s,n,"ol"))return ASTER_TAG_OL;if(name_eq(s,n,"li"))return ASTER_TAG_LI;
    if(name_eq(s,n,"strong")||name_eq(s,n,"b"))return ASTER_TAG_STRONG;if(name_eq(s,n,"em")||name_eq(s,n,"i"))return ASTER_TAG_EM;if(name_eq(s,n,"code"))return ASTER_TAG_CODE;
    return ASTER_TAG_UNKNOWN;
}
static int is_void(uint8_t tag){return tag==ASTER_TAG_BR;}
static int is_block(uint8_t tag){return tag==ASTER_TAG_BODY||tag==ASTER_TAG_H1||tag==ASTER_TAG_H2||tag==ASTER_TAG_P||tag==ASTER_TAG_DIV||tag==ASTER_TAG_UL||tag==ASTER_TAG_OL||tag==ASTER_TAG_LI;}

void aster_document_init(AsterDocument*d){
    if(!d)return;d->node_count=0;d->paint_count=0;d->text_used=0;d->document_height=0;d->title[0]=0;
    add_node(d,-1,ASTER_NODE_ROOT,ASTER_TAG_UNKNOWN,0,0);
}
static size_t decode_text(char*out,size_t cap,const char*s,size_t n){
    size_t o=0;int pending_space=0;
    for(size_t i=0;i<n&&o+1<cap;i++){
        char c=s[i];
        if(c=='&'){
            if(i+3<n&&starts_ci(s+i,"&lt;")){c='<';i+=3;}else if(i+3<n&&starts_ci(s+i,"&gt;")){c='>';i+=3;}
            else if(i+4<n&&starts_ci(s+i,"&amp;")){c='&';i+=4;}else if(i+5<n&&starts_ci(s+i,"&nbsp;")){c=' ';i+=5;}
        }
        if(is_space(c)){pending_space=1;continue;}
        if(pending_space&&o&&o+1<cap)out[o++]=' ';
        pending_space=0;out[o++]=c;
    }
    out[o]=0;return o;
}
static void parse_href(AsterDocument*d,AsterNode*n,const char *s,size_t len){
    size_t i=0;while(i<len){while(i<len&&is_space(s[i]))i++;size_t ns=i;while(i<len&&!is_space(s[i])&&s[i]!='=')i++;size_t nn=i-ns;while(i<len&&is_space(s[i]))i++;if(i>=len||s[i]!='='){while(i<len&&!is_space(s[i]))i++;continue;}i++;while(i<len&&is_space(s[i]))i++;char q=0;if(i<len&&(s[i]=='\''||s[i]=='\"'))q=s[i++];size_t vs=i;if(q){while(i<len&&s[i]!=q)i++;}else while(i<len&&!is_space(s[i])&&s[i]!='>')i++;size_t vn=i-vs;if(q&&i<len)i++;if(name_eq(s+ns,nn,"href")&&vn){n->href_off=store_bytes(d,s+vs,vn);n->href_len=(uint16_t)vn;return;}}
}
int aster_parse_html(AsterDocument*d,const char*html){
    if(!d||!html)return 0;aster_document_init(d);int stack[32],sp=1;stack[0]=0;const char*p=html;
    while(*p&&d->node_count<ASTER_MAX_NODES){
        if(*p!='<'){
            const char*st=p;while(*p&&*p!='<')p++;char tmp[768];size_t n=decode_text(tmp,sizeof(tmp),st,(size_t)(p-st));if(n){uint16_t off=store_bytes(d,tmp,n);int idx=add_node(d,stack[sp-1],ASTER_NODE_TEXT,ASTER_TAG_UNKNOWN,off,(uint16_t)n);if(idx>=0&&d->nodes[stack[sp-1]].tag==ASTER_TAG_TITLE&&d->title[0]==0){size_t m=n<sizeof(d->title)-1?n:sizeof(d->title)-1;for(size_t k=0;k<m;k++)d->title[k]=tmp[k];d->title[m]=0;}}continue;
        }
        if(starts_ci(p,"<!--")){p+=4;while(*p&&!(p[0]=='-'&&p[1]=='-'&&p[2]=='>'))p++;if(*p)p+=3;continue;}
        if(p[1]=='!'||p[1]=='?'){while(*p&&*p!='>')p++;if(*p)p++;continue;}
        p++;int closing=0;if(*p=='/'){closing=1;p++;}while(*p&&is_space(*p))p++;const char*name=p;while(*p&&!is_space(*p)&&*p!='>'&&*p!='/')p++;size_t nn=(size_t)(p-name);uint8_t tag=tag_id(name,nn);
        if(closing){while(*p&&*p!='>')p++;if(*p)p++;for(int i=sp-1;i>0;i--)if(d->nodes[stack[i]].tag==tag){sp=i;break;}continue;}
        const char*attrs=p;while(*p&&*p!='>')p++;size_t an=(size_t)(p-attrs);int self_close=an&&attrs[an-1]=='/';if(*p)p++;
        int idx=add_node(d,stack[sp-1],ASTER_NODE_ELEMENT,tag,0,0);if(idx<0)break;if(tag==ASTER_TAG_A)parse_href(d,&d->nodes[idx],attrs,an);
        if(!self_close&&!is_void(tag)&&sp<(int)(sizeof(stack)/sizeof(stack[0])))stack[sp++]=idx;
    }
    if(!d->title[0]){const char*fallback="Untitled page";size_t i=0;while(fallback[i]&&i+1<sizeof(d->title)){d->title[i]=fallback[i];i++;}d->title[i]=0;}
    return d->node_count>1;
}

static uint8_t inherited_tag(const AsterDocument*d,int node,uint8_t wanted){for(int n=node;n>=0;n=d->nodes[n].parent)if(d->nodes[n].tag==wanted)return 1;return 0;}
static const AsterNode *link_ancestor(const AsterDocument*d,int node){for(int n=node;n>=0;n=d->nodes[n].parent)if(d->nodes[n].tag==ASTER_TAG_A&&d->nodes[n].href_len)return &d->nodes[n];return 0;}
static int hidden_node(const AsterDocument*d,int node){return inherited_tag(d,node,ASTER_TAG_HEAD)||inherited_tag(d,node,ASTER_TAG_TITLE);}
static void emit_item(AsterDocument*d,int x,int y,int w,int h,uint8_t scale,uint8_t flags,uint32_t color,uint16_t off,uint16_t len,const AsterNode*link){
    if(d->paint_count>=ASTER_MAX_PAINT||!len)return;AsterPaintItem*i=&d->paint[d->paint_count++];i->x=(int16_t)x;i->y=(int16_t)y;i->w=(int16_t)w;i->h=(int16_t)h;i->scale=scale;i->flags=flags;i->color=color;i->text_off=off;i->text_len=len;i->href_off=link?link->href_off:0;i->href_len=link?link->href_len:0;
}
static int line_height(uint8_t scale){return scale==2?52:24;}
static int char_width(uint8_t scale){return scale==2?24:11;}
static void layout_text_node(AsterDocument*d,int node,int *cx,int *cy,int left,int width,uint8_t scale,uint32_t color,uint8_t flags){
    AsterNode*n=&d->nodes[node];const char*s=d->text+n->text_off;int cw=char_width(scale),lh=line_height(scale),right=left+width;uint16_t pos=0;const AsterNode*link=link_ancestor(d,node);if(link){flags|=ASTER_FLAG_LINK;color=0x2F74B7;}
    while(pos<n->text_len){while(pos<n->text_len&&s[pos]==' ')pos++;if(pos>=n->text_len)break;uint16_t ws=pos;while(pos<n->text_len&&s[pos]!=' ')pos++;uint16_t we=pos;int ww=(we-ws)*cw;
        if(*cx>left&&*cx+ww>right){*cx=left;*cy+=lh;}
        uint16_t ls=ws,le=we;while(pos<n->text_len){uint16_t save=pos;while(pos<n->text_len&&s[pos]==' ')pos++;if(pos>=n->text_len)break;uint16_t ns=pos;while(pos<n->text_len&&s[pos]!=' ')pos++;uint16_t ne=pos;int add=(int)(ne-ls)*cw;if(*cx+add>right){pos=save;break;}le=ne;}
        int px=*cx;int pw=(le-ls)*cw;emit_item(d,px,*cy,pw,lh,scale,flags,color,(uint16_t)(n->text_off+ls),(uint16_t)(le-ls),link);*cx+=pw+cw;
    }
}
static void layout_walk(AsterDocument*d,int node,int *cx,int *cy,int left,int width){
    if(node<0||hidden_node(d,node))return;AsterNode*n=&d->nodes[node];uint8_t tag=n->tag;int block=is_block(tag);uint8_t scale=1,flags=0;uint32_t color=0x26394B;
    if(tag==ASTER_TAG_H1){scale=2;color=0x1D3550;}else if(tag==ASTER_TAG_H2){flags|=ASTER_FLAG_BOLD;color=0x244E73;}if(inherited_tag(d,node,ASTER_TAG_STRONG))flags|=ASTER_FLAG_BOLD;if(inherited_tag(d,node,ASTER_TAG_CODE))color=0x7B3F57;
    if(block&&*cx!=left){*cx=left;*cy+=24;}if(tag==ASTER_TAG_H1||tag==ASTER_TAG_H2||tag==ASTER_TAG_P||tag==ASTER_TAG_DIV||tag==ASTER_TAG_LI)*cy+=8;if(tag==ASTER_TAG_LI){/* reserve visual indent */*cx=left+22;}
    if(n->type==ASTER_NODE_TEXT)layout_text_node(d,node,cx,cy,left+(inherited_tag(d,node,ASTER_TAG_LI)?22:0),width-(inherited_tag(d,node,ASTER_TAG_LI)?22:0),scale,color,flags);
    if(tag==ASTER_TAG_BR){*cx=left;*cy+=24;}
    for(int c=n->first_child;c>=0;c=d->nodes[c].next_sibling)layout_walk(d,c,cx,cy,left,width);
    if(block&&n->type==ASTER_NODE_ELEMENT){*cx=left;*cy+=tag==ASTER_TAG_H1?20:tag==ASTER_TAG_H2?12:8;}
}
void aster_layout(AsterDocument*d,int viewport_width){
    if(!d)return;if(viewport_width<120)viewport_width=120;d->paint_count=0;int x=0,y=0;layout_walk(d,0,&x,&y,0,viewport_width);d->document_height=y+32;
}
void aster_paint(const AsterDocument*d,int x,int y,int width,int height,int scroll_y){
    if(!d)return;for(unsigned k=0;k<d->paint_count;k++){const AsterPaintItem*i=&d->paint[k];int py=y+i->y-scroll_y;if(py+i->h<y||py>y+height)continue;if(i->x>=width)continue;char tmp[160];size_t n=i->text_len<sizeof(tmp)-1?i->text_len:sizeof(tmp)-1;for(size_t j=0;j<n;j++)tmp[j]=d->text[i->text_off+j];tmp[n]=0;uint32_t drawn;if(i->scale==2)drawn=gfx_display_text((uint32_t)(x+i->x),(uint32_t)py,tmp,i->color);else drawn=gfx_text((uint32_t)(x+i->x),(uint32_t)py,tmp,i->color,1);if(i->flags&ASTER_FLAG_LINK)gfx_line_h((uint32_t)(x+i->x),(uint32_t)(py+i->h-4),drawn,0x6BA5D8);}
}
const char *aster_version(void){return ASTER_VERSION;}
