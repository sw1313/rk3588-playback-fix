// SPDX-License-Identifier: MIT
// Scratch allocation only. Never submitted to HWC, no live buffer metadata edits.
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <cstdio>
int main(){
 int errors=0;int widths[]={64,1920,3840};uint64_t usages[]={0x300,0xb00,0x1a00,0x1b00,0xb33};
 for(int width:widths)for(uint64_t usage:usages){
  android::sp<android::GraphicBuffer> b=new android::GraphicBuffer(width,width==64?64:width*9/16,android::PixelFormat(43),1,usage,"Rgb10ClientScratch");
  uint32_t fourcc=0;uint64_t modifier=0,bytes=0;int rc=b->initCheck();auto& mapper=android::GraphicBufferMapper::get();
  if(!rc)rc=mapper.getPixelFormatFourCC(b->handle,&fourcc);
  if(!rc)rc=mapper.getPixelFormatModifier(b->handle,&modifier);
  if(!rc)rc=mapper.getAllocationSize(b->handle,&bytes);
  printf("RGB10_ALLOC width=%d usage=%llx init=%d query=%d fourcc=%08x modifier=%llx bytes=%llu\n",width,(unsigned long long)usage,b->initCheck(),rc,fourcc,(unsigned long long)modifier,(unsigned long long)bytes);
  errors+=rc || (fourcc!=0x30334241 && fourcc!=0x30335241 && fourcc!=0x30334258 && fourcc!=0x30335258);
 }
 return errors?1:0;
}
