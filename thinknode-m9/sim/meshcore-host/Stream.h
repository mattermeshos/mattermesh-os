#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
// Byte-stream base used by MeshCore's Identity read/write/print helpers; nothing is wired on the host.
class Print {public:virtual ~Print(){}virtual size_t write(uint8_t c){return 1;}virtual size_t write(const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)write(b[i]);return n;}
 size_t print(const char *s){return write(reinterpret_cast<const uint8_t*>(s),strlen(s));}size_t println(const char *s=""){size_t n=print(s);return n+print("\n");}
 size_t print(char c){return write(uint8_t(c));}size_t print(int v){char t[16];snprintf(t,sizeof t,"%d",v);return print(t);}size_t printf(const char *,...){return 0;}};
class Stream:public Print {public:virtual int available(){return 0;}virtual int read(){return -1;}virtual int peek(){return -1;}
 size_t readBytes(uint8_t *b,size_t n){size_t k=0;while(k<n){int c=read();if(c<0)break;b[k++]=uint8_t(c);}return k;}size_t readBytes(char *b,size_t n){return readBytes(reinterpret_cast<uint8_t*>(b),n);}};
