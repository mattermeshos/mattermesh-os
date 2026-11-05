#pragma once
#include <stdint.h>
// Sequential PackBits reader for the lock-screen art (tools/packbits.py): each call fills the next n bytes.
struct PackBitsReader {
 const uint8_t *p;int left=0;bool repeat=false;uint8_t value=0;
 explicit PackBitsReader(const uint8_t *data):p(data){}
 void read(uint8_t *out,int n){
  while(n>0){
   if(!left){uint8_t h=*p++;if(h==128)continue;repeat=h>128;left=repeat?257-h:h+1;if(repeat)value=*p++;}
   int k=left<n?left:n;
   if(repeat)for(int i=0;i<k;i++)*out++=value;else for(int i=0;i<k;i++)*out++=*p++;
   left-=k;n-=k;
  }
 }
};
