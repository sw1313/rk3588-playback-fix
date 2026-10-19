// SPDX-License-Identifier: MIT
// Real factory PopulateFB/Init in isolated preview objects. No HAL/device or
// actual fences opened; the buffer/fence fields must remain byte-for-byte.
#include "../../magisk-hdr/src/hwc_bridge.cpp"
#include <cstdlib>
#include <ui/GraphicBuffer.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static bool ready=true;
static bool fakeReady(){return ready;}
static void put(void* p,size_t offset,const void* v,size_t n){memcpy(static_cast<char*>(p)+offset,v,n);}
static int (*factoryIntProperty)(const char*,const char*)=nullptr;
static int referencePolicy=0;
static int validationResult=0;
static unsigned failures=0;
static bool expectedImport=false;
static void fakeFailure(){++failures;}
static void checkScope(){
 CHECK(factoryIntProperty("vendor.gralloc.no_afbc_for_fb_target_layer","0")==referencePolicy);
 CHECK(factoryIntProperty("debug.rkhdr.fixture.absent","37")==37);
}
static int32_t fakeValidate(hwc2_device_t*,hwc2_display_t,uint32_t*,uint32_t*){checkScope();return validationResult;}
static int32_t fakePresent(hwc2_device_t*,hwc2_display_t,int32_t* fence){CHECK(importingHdrTarget==expectedImport);checkScope();*fence=73;return HWC2_ERROR_NO_RESOURCES;}
static hwc2_function_pointer_t fakeGet(hwc2_device_t*,int32_t descriptor){
 if(descriptor==HWC2_FUNCTION_VALIDATE_DISPLAY)return reinterpret_cast<hwc2_function_pointer_t>(fakeValidate);
 if(descriptor==HWC2_FUNCTION_PRESENT_DISPLAY)return reinterpret_cast<hwc2_function_pointer_t>(fakePresent);
 return nullptr;
}
int main(int argc,char** argv){
 CHECK(argc==2);auto module=loadOriginal(argv[1]);CHECK(module);
 Slot slot{};CHECK(prepareTargetPlan(module,&slot));
 auto before=reinterpret_cast<PopulateFb>(slot.saved);CHECK(installTargetPlan(module));
 auto after=*reinterpret_cast<PopulateFb*>(slot.pointer);CHECK(before!=after);
 Slot add{},commit{},release{},hdrSwitch{},prop{};CHECK(slots(module,&add,&commit,&release,&hdrSwitch,&prop));
 realPropertyGet=reinterpret_cast<decltype(realPropertyGet)>(prop.saved);CHECK(writeSlot(prop,reinterpret_cast<void*>(hookedPropertyGet)));
 Dl_info info{};CHECK(dladdr(module,&info));auto lib=dlopen(info.dli_fname,RTLD_NOW|RTLD_LOCAL|RTLD_NOLOAD);CHECK(lib);
 factoryIntProperty=reinterpret_cast<decltype(factoryIntProperty)>(dlsym(lib,"_ZN7android20hwc_get_int_propertyEPKcS1_"));CHECK(factoryIntProperty);
 referencePolicy=factoryIntProperty("vendor.gralloc.no_afbc_for_fb_target_layer","0");originalGet=fakeGet;
 sfTargetReady=fakeReady;targetPlanFailure=fakeFailure;unsigned cases=0;
 for(int mode:{int(HAL_COLOR_MODE_NATIVE),int(HAL_COLOR_MODE_SRGB),int(HAL_COLOR_MODE_BT2100_PQ),int(HAL_COLOR_MODE_BT2100_HLG)})
 for(bool enabled:{false,true})for(bool available:{false,true})for(uint64_t display:{0ULL,1ULL}){
  alignas(16) unsigned char self[0x200]{},ctx[0x200]{},reference[0x200]{},candidate[0x200]{};
  float alpha=1,crop[4]={0,0,3840,2160};int frame[4]={0,0,3840,2160},width=3840,height=2160;
  put(self,0x58,frame,sizeof(frame));put(self,0x68,&alpha,4);put(self,0x6c,crop,sizeof(crop));put(ctx,8,&width,4);put(ctx,12,&height,4);
  before(self,0,reference,ctx,123,true);
  clientHdr10=enabled;ready=available;clientColorMode=mode;primaryFbSelf=nullptr;validation={reinterpret_cast<hwc2_device_t*>(1),display};
  after(self,0,candidate,ctx,123,true);
  bool hdr=enabled && available && display==0 && (mode==HAL_COLOR_MODE_BT2100_PQ || mode==HAL_COLOR_MODE_BT2100_HLG);
  for(int rc:{HWC2_ERROR_NONE,HWC2_ERROR_HAS_CHANGES,HWC2_ERROR_BAD_CONFIG,HWC2_ERROR_BAD_DISPLAY}){
   targetPlanFailed=false;failures=0;validationResult=rc;expectedImport=hdr;hwc2_device_t device{};int32_t fence=-9;
   bool rejected=hdr && rc!=HWC2_ERROR_NONE && rc!=HWC2_ERROR_HAS_CHANGES;
   CHECK(validate(&device,display,nullptr,nullptr)==rc);
   CHECK(targetPlanFailed==rejected && failures==unsigned(rejected));
   CHECK(present(&device,display,&fence)==(rejected?HWC2_ERROR_NOT_VALIDATED:HWC2_ERROR_NO_RESOURCES));
   CHECK(fence==(rejected?-9:73));
   CHECK(!importingHdrTarget);
  }
  CHECK(factoryIntProperty("vendor.gralloc.no_afbc_for_fb_target_layer","0")==referencePolicy);
  if(!hdr)CHECK(!memcmp(reference,candidate,sizeof(candidate)));
  else {
   CHECK(fbLoad<int32_t>(candidate,0x124)==43 && fbLoad<uint32_t>(candidate,0x140)==0x30334241U);
   CHECK(fbLoad<uint64_t>(candidate,0x148)==0x0800000000000001ULL && fbLoad<uint8_t>(candidate,0x109)==1);
   CHECK(fbLoad<int32_t>(candidate,0x17c)==(mode==HAL_COLOR_MODE_BT2100_PQ?HAL_DATASPACE_BT2020_PQ:HAL_DATASPACE_BT2020_HLG));
   CHECK(fbLoad<uint16_t>(candidate,0x184)==(mode==HAL_COLOR_MODE_BT2100_PQ?2:0));
   CHECK(!memcmp(reference,candidate,0xf0)); // Handle/fence, crop, blending and geometry.
  }
  cases++;
 }
 unsigned imports=0;auto& mapper=android::GraphicBufferMapper::get();
 for(int width:{64,1920,3840}){
  int height=width==64?64:width*9/16;
  android::sp<android::GraphicBuffer> b=new android::GraphicBuffer(width,height,android::PixelFormat(43),1,0xb00,"HdrMapperLayoutFixture");
  CHECK(b->initCheck()==0);uint32_t fourcc=0;uint64_t modifier=0;
  CHECK(!mapper.getPixelFormatFourCC(b->handle,&fourcc) && fourcc==0x30334241U);
  CHECK(!mapper.getPixelFormatModifier(b->handle,&modifier) && modifier==0x0800000000000001ULL);
  auto import=[&](bool scoped,int w,int h,int format,uint64_t usage,int stride){
   importingHdrTarget=scoped;buffer_handle_t out=nullptr;
   int rc=mapper.importBuffer(b->handle,w,h,1,android::PixelFormat(format),usage,stride,&out);
   if(!rc){CHECK(out);CHECK(!mapper.freeBuffer(out));}
   importingHdrTarget=false;++imports;return rc;
  };
  CHECK(import(false,width,height,43,0xb00,0)==0);
  CHECK(import(false,width,height,43,0x1a00,0)!=0); // Actual old selector mismatch.
  CHECK(import(true,width,height,43,0x1a00,0)==0);
  CHECK(import(true,width+1,height,43,0x1a00,0)!=0);
  CHECK(import(true,width,height+1,43,0x1a00,0)!=0);
  CHECK(import(true,width,height,1,0x1a00,0)!=0);
  CHECK(import(true,width,height,43,0x1a00,b->getStride()+1)!=0);
  CHECK(import(false,width,height,43,0x1a00,0)!=0); // Scope restored.
 }
 printf("PASS actual mapper import cases=%u original_FB_rejects_RGB10_AFBC=1 corrected_FB_accepts=1 invalid_dimensions_format_stride_rejected=1 scope_restored=1\n",imports);
 printf("PASS actual PopulateFB/Init cases=%u HDR_AFBC_plan=1 validation_error_cases=128 original_handle_fence_geometry_preserved=1 SDR_virtual_prerequisite_passthrough=1 factory_VOP_property_unchanged=1 callback_return_fence_preserved=1 rejected_HDR_no_present_no_fabricated_fence=1 HAL_opened=0\n",cases);
}
