#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace matter {
// Packet path hashes are short identifiers, not authenticated delivery ACKs.
struct RepeaterObserver {
 uint8_t hashes[64][4]={},sizes[64]={};int count=0;
 bool observe(const uint8_t *path,unsigned hops,unsigned size){
  if(!size||size>4||hops*size>64)return false;
  int old=count;
  for(unsigned i=0;i<hops;i++){auto h=path+i*size;bool found=false;
   for(int j=0;j<count;j++)if(sizes[j]==size&&!memcmp(hashes[j],h,size)){found=true;break;}
   if(!found&&count<64){memcpy(hashes[count],h,size);sizes[count++]=size;}
  }
  return old!=count;
 }
};
}
