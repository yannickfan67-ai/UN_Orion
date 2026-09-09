from pathlib import Path


def rep(path, old, new):
    p=Path(path); s=p.read_text()
    if old not in s:
        raise SystemExit(f"missing snippet in {path}: {old[:90]!r}")
    p.write_text(s.replace(old,new))

# Engine selector kinds and expanded node initialization.
rep("kernel/aster.c",
    "#define CSS_ANY_TAG 255u\ntypedef struct{uint8_t flags,scale,color_set;uint32_t color;} Style;",
    "#define CSS_ANY_TAG 255u\n#define CSS_SELECTOR_ANY 0u\n#define CSS_SELECTOR_TAG 1u\n#define CSS_SELECTOR_CLASS 2u\n#define CSS_SELECTOR_ID 3u\n#define CSS_SELECTOR_TAG_CLASS 4u\n#define CSS_SELECTOR_TAG_ID 5u\ntypedef struct{uint8_t flags,scale,color_set;uint32_t color;} Style;")
rep("kernel/aster.c",
    "n->href_off=n->href_len=n->src_off=n->src_len=n->width_hint=n->height_hint=0;",
    "n->href_off=n->href_len=n->src_off=n->src_len=n->id_off=n->id_len=n->class_off=n->class_len=n->width_hint=n->height_hint=0;")

old_css='''static void css(AsterDocument*d,const char*s,size_t n){size_t p=0;while(p<n&&d->css_count<ASTER_MAX_CSS_RULES){while(p<n&&sp(s[p]))p++;size_t ss=p;while(p<n&&s[p]!='{')p++;if(p>=n)break;size_t sn=p-ss;p++;size_t ds=p;while(p<n&&s[p]!='}')p++;size_t dn=p-ds;if(p<n)p++;while(sn&&sp(s[ss+sn-1]))sn--;while(sn&&sp(s[ss])){ss++;sn--;}uint8_t t=(sn==1&&s[ss]=='*')?CSS_ANY_TAG:tagid(s+ss,sn);if(t==ASTER_TAG_UNKNOWN)continue;Style st=styletext(s+ds,dn);AsterCssRule*r=&d->css[d->css_count++];r->tag=t;r->flags=st.flags;r->scale=st.scale;r->color_set=st.color_set;r->color=st.color;}}'''
new_css='''static int selector(AsterDocument*d,const char*s,size_t n,AsterCssRule*r){while(n&&sp(*s)){s++;n--;}while(n&&sp(s[n-1]))n--;if(!n)return 0;for(size_t i=0;i<n;i++)if(sp(s[i])||s[i]=='>'||s[i]=='['||s[i]==':'||s[i]=='+')return 0;r->tag=CSS_ANY_TAG;r->selector_kind=CSS_SELECTOR_ANY;r->selector_off=r->selector_len=0;if(n==1&&s[0]=='*')return 1;size_t split=n;char mark=0;for(size_t i=0;i<n;i++)if(s[i]=='.'||s[i]=='#'){split=i;mark=s[i];break;}if(split==0){if(n<2)return 0;r->selector_kind=mark=='.'?CSS_SELECTOR_CLASS:CSS_SELECTOR_ID;r->selector_off=store(d,s+1,n-1);r->selector_len=(uint16_t)(n-1);return r->selector_len!=0;}uint8_t t=tagid(s,split);if(t==ASTER_TAG_UNKNOWN)return 0;r->tag=t;if(!mark){r->selector_kind=CSS_SELECTOR_TAG;return 1;}if(split+1>=n)return 0;r->selector_kind=mark=='.'?CSS_SELECTOR_TAG_CLASS:CSS_SELECTOR_TAG_ID;r->selector_off=store(d,s+split+1,n-split-1);r->selector_len=(uint16_t)(n-split-1);return r->selector_len!=0;}
static void cssadd(AsterDocument*d,const char*s,size_t n,Style st){if(d->css_count>=ASTER_MAX_CSS_RULES)return;AsterCssRule r;r.flags=st.flags;r.scale=st.scale;r.color_set=st.color_set;r.color=st.color;if(!selector(d,s,n,&r))return;d->css[d->css_count++]=r;}
static void css(AsterDocument*d,const char*s,size_t n){size_t p=0;while(p<n&&d->css_count<ASTER_MAX_CSS_RULES){while(p<n&&sp(s[p]))p++;size_t ss=p;while(p<n&&s[p]!='{')p++;if(p>=n)break;size_t sn=p-ss;p++;size_t ds=p;while(p<n&&s[p]!='}')p++;size_t dn=p-ds;if(p<n)p++;Style st=styletext(s+ds,dn);size_t q=0;while(q<sn&&d->css_count<ASTER_MAX_CSS_RULES){size_t a=q;while(q<sn&&s[ss+q]!=',')q++;cssadd(d,s+ss+a,q-a,st);if(q<sn&&s[ss+q]==',')q++;}}}'''
rep("kernel/aster.c",old_css,new_css)

rep("kernel/aster.c",
    "if(attr(s,len,\"src\",&v,&z)&&z){n->src_off=store(d,v,z);n->src_len=(uint16_t)z;}if(attr(s,len,\"alt\",&v,&z)&&z)",
    "if(attr(s,len,\"src\",&v,&z)&&z){n->src_off=store(d,v,z);n->src_len=(uint16_t)z;}if(attr(s,len,\"id\",&v,&z)&&z){n->id_off=store(d,v,z);n->id_len=(uint16_t)z;}if(attr(s,len,\"class\",&v,&z)&&z){n->class_off=store(d,v,z);n->class_len=(uint16_t)z;}if(attr(s,len,\"alt\",&v,&z)&&z)")

old_res='''static Style resolved(const AsterDocument*d,int n){Style st={0,1,0,0x26394B};uint8_t tag=d->nodes[n].tag;if(inh(d,n,ASTER_TAG_H1)){st.scale=2;st.flags|=ASTER_STYLE_BOLD;st.color=0x1D3550;st.color_set=1;}else if(inh(d,n,ASTER_TAG_H2)||inh(d,n,ASTER_TAG_H3))st.flags|=ASTER_STYLE_BOLD;if(inh(d,n,ASTER_TAG_STRONG))st.flags|=ASTER_STYLE_BOLD;if(inh(d,n,ASTER_TAG_CODE)){st.color=0x7B3F57;st.color_set=1;}for(uint8_t i=0;i<d->css_count;i++){const AsterCssRule*r=&d->css[i];if(r->tag!=CSS_ANY_TAG&&r->tag!=tag)continue;st.flags|=r->flags;if(r->scale)st.scale=r->scale;if(r->color_set){st.color=r->color;st.color_set=1;}}for(int x=n;x>=0;x=d->nodes[x].parent){const AsterNode*q=&d->nodes[x];st.flags|=q->style_flags;if(q->style_scale)st.scale=q->style_scale;if(q->style_color){st.color=q->style_color;st.color_set=1;}}return st;}'''
new_res='''static int teq(const AsterDocument*d,uint16_t a,uint16_t an,uint16_t b,uint16_t bn){if(an!=bn||(size_t)a+an>ASTER_TEXT_CAP||(size_t)b+bn>ASTER_TEXT_CAP)return 0;for(uint16_t i=0;i<an;i++)if(d->text[a+i]!=d->text[b+i])return 0;return 1;}
static int hasclass(const AsterDocument*d,const AsterNode*n,const AsterCssRule*r){if(!n->class_len||(size_t)n->class_off+n->class_len>ASTER_TEXT_CAP)return 0;uint16_t p=0;while(p<n->class_len){while(p<n->class_len&&sp(d->text[n->class_off+p]))p++;uint16_t a=p;while(p<n->class_len&&!sp(d->text[n->class_off+p]))p++;uint16_t z=(uint16_t)(p-a);if(z==r->selector_len){int ok=1;for(uint16_t i=0;i<z;i++)if(d->text[n->class_off+a+i]!=d->text[r->selector_off+i]){ok=0;break;}if(ok)return 1;}}return 0;}
static int matches(const AsterDocument*d,const AsterCssRule*r,const AsterNode*n){if(r->tag!=CSS_ANY_TAG&&r->tag!=n->tag)return 0;switch(r->selector_kind){case CSS_SELECTOR_ANY:case CSS_SELECTOR_TAG:return 1;case CSS_SELECTOR_CLASS:case CSS_SELECTOR_TAG_CLASS:return hasclass(d,n,r);case CSS_SELECTOR_ID:case CSS_SELECTOR_TAG_ID:return n->id_len&&teq(d,n->id_off,n->id_len,r->selector_off,r->selector_len);default:return 0;}}
static uint8_t specof(const AsterCssRule*r){switch(r->selector_kind){case CSS_SELECTOR_ID:return 100;case CSS_SELECTOR_TAG_ID:return 101;case CSS_SELECTOR_CLASS:return 10;case CSS_SELECTOR_TAG_CLASS:return 11;case CSS_SELECTOR_TAG:return 1;default:return 0;}}
static void rules(const AsterDocument*d,int node,Style*st){const AsterNode*n=&d->nodes[node];uint8_t cs=0,ss=0;int co=-1,so=-1;for(uint8_t i=0;i<d->css_count;i++){const AsterCssRule*r=&d->css[i];if(!matches(d,r,n))continue;uint8_t spc=specof(r);st->flags|=r->flags;if(r->scale&&(spc>ss||(spc==ss&&(int)i>=so))){st->scale=r->scale;ss=spc;so=i;}if(r->color_set&&(spc>cs||(spc==cs&&(int)i>=co))){st->color=r->color;st->color_set=1;cs=spc;co=i;}}}
static Style resolved(const AsterDocument*d,int n){Style st={0,1,0,0x26394B};if(inh(d,n,ASTER_TAG_H1)){st.scale=2;st.flags|=ASTER_STYLE_BOLD;st.color=0x1D3550;st.color_set=1;}else if(inh(d,n,ASTER_TAG_H2)||inh(d,n,ASTER_TAG_H3))st.flags|=ASTER_STYLE_BOLD;if(inh(d,n,ASTER_TAG_STRONG))st.flags|=ASTER_STYLE_BOLD;if(inh(d,n,ASTER_TAG_CODE)){st.color=0x7B3F57;st.color_set=1;}int chain[48],count=0;for(int x=n;x>=0&&count<48;x=d->nodes[x].parent)chain[count++]=x;for(int i=count-1;i>=0;i--){const AsterNode*q=&d->nodes[chain[i]];rules(d,chain[i],&st);st.flags|=q->style_flags;if(q->style_scale)st.scale=q->style_scale;if(q->style_color){st.color=q->style_color;st.color_set=1;}}return st;}'''
rep("kernel/aster.c",old_res,new_res)

# Permanent smoke target in the normal build system.
rep("Makefile",
    ".PHONY: all clean run image iso kernel smoke iso-smoke install-smoke network-smoke legacy-i686 legacy-smoke",
    ".PHONY: all clean run image iso kernel smoke iso-smoke install-smoke network-smoke legacy-i686 legacy-smoke aster-smoke")
rep("Makefile","kernel: $(BUILD)/kernel.elf\n",
    "kernel: $(BUILD)/kernel.elf\n\naster-smoke: | $(BUILD)\n\t$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude kernel/aster.c tests/aster_selector_smoke.c -o $(BUILD)/aster-selector-smoke\n\t./$(BUILD)/aster-selector-smoke\n")
rep(".github/workflows/build.yml",
    "      - name: Build boot media\n",
    "      - name: Aster selector host smoke\n        run: make aster-smoke\n      - name: Build boot media\n")
