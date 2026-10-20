// SPDX-License-Identifier: MIT
// Firmware identity is independent of ramdisk compression and local root config.
#ifndef RK_COLD_FIRMWARE_H
#define RK_COLD_FIRMWARE_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cold_sha256.h"
#include "cold_firmware_contract.h"
static inline uint32_t rk_boot_u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline uint64_t rk_boot_align(uint64_t n){return (n+2047)&~2047ULL;}
static inline int rk_boot_cmdline(char *cmd,int candidate){
 char base[1536];snprintf(base,sizeof(base),"%s",RK_BOOT_BASE_CMD);
 char *tokens[96],*required[96];unsigned n=0,r=0;char *save=NULL,*p=strtok_r(cmd," \t\r\n",&save);
 while(p){if(n==96)return 0;tokens[n++]=p;p=strtok_r(NULL," \t\r\n",&save);}
 p=strtok_r(base," ",&save);while(p){if(r==96)return 0;required[r++]=p;p=strtok_r(NULL," ",&save);}
 unsigned blacklisted=0;
 for(unsigned i=0;i<n;i++){
  size_t key=strcspn(tokens[i],"=");
  for(unsigned j=0;j<i;j++)if(key==strcspn(tokens[j],"=")&&!strncmp(tokens[i],tokens[j],key))return 0;
  int known=0;for(unsigned j=0;j<r;j++)if(!strcmp(tokens[i],required[j]))known=1;
  for(unsigned j=0;j<sizeof(rk_boot_extra_cmd)/sizeof(rk_boot_extra_cmd[0]);j++)if(!strcmp(tokens[i],rk_boot_extra_cmd[j]))known=1;
  if(!strcmp(tokens[i],RK_BOOT_BLACKLIST)){if(!candidate)return 0;known=1;blacklisted++;}
  if(!known)return 0;
 }
 for(unsigned j=0;j<r;j++){int found=0;for(unsigned i=0;i<n;i++)if(!strcmp(tokens[i],required[j]))found=1;if(!found)return 0;}
 return candidate?blacklisted==1:blacklisted==0;
}
static inline int rk_boot_region(FILE *f,uint64_t start,uint32_t bytes,const char *expected){
 if(fseek(f,(long)start,SEEK_SET))return 0;rk_sha256 s;rk_sha256_init(&s);unsigned char buf[32768];
 while(bytes){size_t take=bytes>sizeof(buf)?sizeof(buf):bytes;if(fread(buf,1,take,f)!=take)return 0;rk_sha256_update(&s,buf,take);bytes-=(uint32_t)take;}
 char hash[65];rk_sha256_finish(&s,hash);return !strcmp(hash,expected);
}
static inline int rk_boot_firmware(const char *path,int candidate){
 FILE *f=fopen(path,"rb");if(!f)return 0;unsigned char header[2048];int ok=0;
 if(fseek(f,0,SEEK_END)||ftell(f)!=(long)RK_BOOT_BYTES||fseek(f,0,SEEK_SET)||fread(header,1,sizeof(header),f)!=sizeof(header))goto done;
 if(memcmp(header,"ANDROID!",8)||rk_boot_u32(header+36)!=2048||rk_boot_u32(header+40)!=2)goto done;
 uint32_t sizes[]={rk_boot_u32(header+8),rk_boot_u32(header+16),rk_boot_u32(header+24),rk_boot_u32(header+1632),rk_boot_u32(header+1648)};
 if(sizes[0]!=RK_BOOT_PART_0_BYTES||!sizes[1]||sizes[2]!=RK_BOOT_PART_2_BYTES||sizes[3]!=RK_BOOT_PART_3_BYTES||sizes[4]!=RK_BOOT_PART_4_BYTES)goto done;
 char cmd[1537];size_t a=strnlen((char*)header+64,512),b=strnlen((char*)header+608,1024);
 memcpy(cmd,header+64,a);memcpy(cmd+a,header+608,b);cmd[a+b]=0;if(!rk_boot_cmdline(cmd,candidate))goto done;
 memset(header+16,0,4);memset(header+64,0,1568); /* cmdline + image ID; all other header bytes remain pinned */
 rk_sha256 s;char hash[65];rk_sha256_init(&s);rk_sha256_update(&s,header,sizeof(header));rk_sha256_finish(&s,hash);
 if(strcmp(hash,RK_BOOT_FIXED_HEADER))goto done;
 uint64_t offset=2048;
 for(unsigned i=0;i<5;i++){
  if(offset+sizes[i]>RK_BOOT_BYTES)goto done;
  const char *expected=i==0?RK_BOOT_PART_0_SHA:i==2?RK_BOOT_PART_2_SHA:i==3?RK_BOOT_PART_3_SHA:i==4?RK_BOOT_PART_4_SHA:NULL;
  if(expected&&!rk_boot_region(f,offset,sizes[i],expected))goto done;
  offset+=rk_boot_align(sizes[i]);
 }
 if(offset>RK_BOOT_BYTES||fseek(f,(long)offset,SEEK_SET))goto done;
 while(offset<RK_BOOT_BYTES){unsigned char buf[32768];size_t n=RK_BOOT_BYTES-offset>sizeof(buf)?sizeof(buf):(size_t)(RK_BOOT_BYTES-offset);
  if(fread(buf,1,n,f)!=n)goto done;for(size_t i=0;i<n;i++)if(buf[i])goto done;offset+=n;}
 ok=1;
done:fclose(f);return ok;
}
#endif
