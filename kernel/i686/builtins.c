#include <stdint.h>

/* Tiny freestanding 64-bit division runtime for the i686 kernel.
   Clang emits these helpers for uptime/PMM formatting on 32-bit targets. */
static uint64_t udivmod64(uint64_t n,uint64_t d,uint64_t *rem){
    if(!d){ if(rem)*rem=n; return UINT64_MAX; }
    uint64_t q=0,r=0;
    for(int i=63;i>=0;i--){
        r=(r<<1)|((n>>(unsigned)i)&1u);
        if(r>=d){r-=d;q|=(1ULL<<(unsigned)i);}
    }
    if(rem)*rem=r;
    return q;
}
uint64_t __udivdi3(uint64_t n,uint64_t d){return udivmod64(n,d,0);}
uint64_t __umoddi3(uint64_t n,uint64_t d){uint64_t r;udivmod64(n,d,&r);return r;}
uint64_t __udivmoddi4(uint64_t n,uint64_t d,uint64_t *r){return udivmod64(n,d,r);}
void *memcpy(void *dst,const void *src,unsigned long n){unsigned char*d=dst;const unsigned char*s=src;while(n--)*d++=*s++;return dst;}
void *memset(void *dst,int c,unsigned long n){unsigned char*d=dst;while(n--)*d++=(unsigned char)c;return dst;}
void *memmove(void *dst,const void *src,unsigned long n){unsigned char*d=dst;const unsigned char*s=src;if(d<s){while(n--)*d++=*s++;}else{d+=n;s+=n;while(n--)*--d=*--s;}return dst;}
