// SPDX-License-Identifier: MIT
// Local streaming SHA-256 for read-only boot component qualification.
#ifndef RK_COLD_SHA256_H
#define RK_COLD_SHA256_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {uint32_t h[8];uint64_t bytes;size_t used;unsigned char block[64];} rk_sha256;
static inline uint32_t rk_ror(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static inline void rk_sha256_block(rk_sha256 *s,const unsigned char *b){
 static const uint32_t k[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t w[64];for(unsigned i=0;i<16;i++)w[i]=(uint32_t)b[4*i]<<24|(uint32_t)b[4*i+1]<<16|(uint32_t)b[4*i+2]<<8|b[4*i+3];
 for(unsigned i=16;i<64;i++){uint32_t x=w[i-15],y=w[i-2];w[i]=w[i-16]+(rk_ror(x,7)^rk_ror(x,18)^(x>>3))+w[i-7]+(rk_ror(y,17)^rk_ror(y,19)^(y>>10));}
 uint32_t a=s->h[0],c=s->h[2],b0=s->h[1],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
 for(unsigned i=0;i<64;i++){
  uint32_t t=h+(rk_ror(e,6)^rk_ror(e,11)^rk_ror(e,25))+((e&f)^(~e&g))+k[i]+w[i];
  uint32_t u=(rk_ror(a,2)^rk_ror(a,13)^rk_ror(a,22))+((a&b0)^(a&c)^(b0&c));
  h=g;g=f;f=e;e=d+t;d=c;c=b0;b0=a;a=t+u;
 }
 s->h[0]+=a;s->h[1]+=b0;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static inline void rk_sha256_init(rk_sha256 *s){
 static const uint32_t h[]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 memcpy(s->h,h,sizeof(h));s->bytes=0;s->used=0;
}
static inline void rk_sha256_update(rk_sha256 *s,const void *ptr,size_t n){
 const unsigned char *b=ptr;s->bytes+=n;
 while(n){size_t take=64-s->used;if(take>n)take=n;memcpy(s->block+s->used,b,take);s->used+=take;b+=take;n-=take;
  if(s->used==64){rk_sha256_block(s,s->block);s->used=0;}}
}
static inline void rk_sha256_finish(rk_sha256 *s,char out[65]){
 uint64_t bits=s->bytes*8;unsigned char tail[128]={0x80};size_t n=s->used<56?56-s->used:120-s->used;
 for(unsigned i=0;i<8;i++)tail[n+i]=(unsigned char)(bits>>(56-8*i));
 rk_sha256_update(s,tail,n+8);static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;i++){unsigned char v=(unsigned char)(s->h[i/4]>>(24-8*(i%4)));out[2*i]=hex[v>>4];out[2*i+1]=hex[v&15];}out[64]=0;
}
#endif
