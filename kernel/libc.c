#include <stddef.h>
void *memcpy(void *dst,const void *src,size_t n){unsigned char*d=dst;const unsigned char*s=src;while(n--)*d++=*s++;return dst;}
void *memmove(void *dst,const void *src,size_t n){unsigned char*d=dst;const unsigned char*s=src;if(d<s){while(n--)*d++=*s++;}else{d+=n;s+=n;while(n--)*--d=*--s;}return dst;}
int memcmp(const void *a,const void *b,size_t n){const unsigned char*x=a,*y=b;while(n--){if(*x!=*y)return *x<*y?-1:1;x++;y++;}return 0;}
size_t strlen(const char *s){size_t n=0;while(s&&s[n])n++;return n;}
