#pragma once
// mbedtls SHA-256 on top of rweather Crypto (the library the firmware already links).
#include <SHA256.h>
typedef struct {SHA256 *h;} mbedtls_sha256_context;
inline void mbedtls_sha256_init(mbedtls_sha256_context *c){c->h=new SHA256();}
inline int mbedtls_sha256_starts(mbedtls_sha256_context *c,int){c->h->reset();return 0;}
inline int mbedtls_sha256_update(mbedtls_sha256_context *c,const unsigned char *d,size_t n){c->h->update(d,n);return 0;}
inline int mbedtls_sha256_finish(mbedtls_sha256_context *c,unsigned char out[32]){c->h->finalize(out,32);return 0;}
inline void mbedtls_sha256_free(mbedtls_sha256_context *c){delete c->h;c->h=nullptr;}
