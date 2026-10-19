// SPDX-License-Identifier: MIT
// Actual adapter callback tests, no display/HAL/DRM opened.
#include "../../magisk-hdr/src/hwc_bridge.cpp"
#include <cstdlib>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}++checks;}while(0)
static unsigned checks=0;
static bool sinkPq=true,sinkHlg=true;
static bool sfReady=true;
static bool fakeSfReady(){return sfReady;}
static int lastSpace=-1,lastFormat=-1,lastMode=-1;
static int32_t fakeCaps(hwc2_device_t*,hwc2_display_t display,uint32_t *n,int32_t *out,float*,float*,float*) {
 if(display)return HWC2_ERROR_BAD_DISPLAY;
 uint32_t count=unsigned(sinkPq)+unsigned(sinkHlg),cap=*n;
 if(out){uint32_t i=0;if(sinkPq&&i<cap)out[i++]=2;if(sinkHlg&&i<cap)out[i++]=3;*n=i;}else *n=count;
 return 0;
}
static int32_t fakeModes(hwc2_device_t*,hwc2_display_t display,uint32_t *n,int32_t *out){if(display)return HWC2_ERROR_BAD_DISPLAY;if(!out)*n=1;else if(*n){out[0]=0;*n=1;}return 0;}
static int32_t fakeMode(hwc2_device_t*,hwc2_display_t display,int32_t m){lastMode=m;return display?HWC2_ERROR_BAD_DISPLAY:m==0?0:HWC2_ERROR_BAD_PARAMETER;}
static int32_t fakeSupport(hwc2_device_t*,hwc2_display_t display,uint32_t w,uint32_t h,int32_t f,int32_t s){lastSpace=s;lastFormat=f;return display?HWC2_ERROR_BAD_DISPLAY:w>4096||h>4096||!w||!h||s?HWC2_ERROR_UNSUPPORTED:0;}
static hwc2_function_pointer_t fakeGet(hwc2_device_t*,int32_t desc){
 switch(desc){case HWC2_FUNCTION_GET_HDR_CAPABILITIES:return (hwc2_function_pointer_t)fakeCaps;
 case HWC2_FUNCTION_GET_COLOR_MODES:return (hwc2_function_pointer_t)fakeModes;
 case HWC2_FUNCTION_SET_COLOR_MODE:return (hwc2_function_pointer_t)fakeMode;
 case HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT:return (hwc2_function_pointer_t)fakeSupport;default:return nullptr;}
}
int main(){
 originalGet=fakeGet;sfTargetReady=fakeSfReady;hwc2_device_t d{};uint32_t n=0;int32_t out[6]={-9,-9,-9,-9,-9,-9};
 clientHdr10=false;CHECK(colorModes(&d,0,&n,nullptr)==0&&n==1);CHECK(colorMode(&d,0,11)==HWC2_ERROR_BAD_PARAMETER);
 clientHdr10=true;sfReady=false;CHECK(colorModes(&d,0,&n,nullptr)==0&&n==1);
 sfReady=true;CHECK(colorModes(&d,0,&n,nullptr)==0&&n==4);
 for(uint32_t capacity=0;capacity<=6;capacity++){
  n=capacity;std::fill(out,out+6,-9);CHECK(colorModes(&d,0,&n,out)==0&&n==std::min(capacity,4U));
  int expected[]={0,7,11,12};for(unsigned i=0;i<n;i++)CHECK(out[i]==expected[i]);for(unsigned i=n;i<6;i++)CHECK(out[i]==-9);
 }
 CHECK(colorModes(&d,1,&n,out)==HWC2_ERROR_BAD_DISPLAY);CHECK(colorModes(&d,0,nullptr,out)==HWC2_ERROR_BAD_PARAMETER);
 CHECK(colorMode(&d,0,11)==0&&lastMode==0);CHECK(colorMode(&d,0,12)==0);CHECK(colorMode(&d,0,9)==HWC2_ERROR_BAD_PARAMETER);
 CHECK(clientSupport(&d,0,3840,2160,43,HAL_DATASPACE_BT2020_PQ)==0&&lastFormat==43&&lastSpace==0);
 CHECK(clientSupport(&d,0,3840,2160,43,HAL_DATASPACE_BT2020_HLG)==0);
 CHECK(clientSupport(&d,0,3840,2160,1,HAL_DATASPACE_BT2020_PQ)==HWC2_ERROR_UNSUPPORTED);
 CHECK(clientSupport(&d,0,3840,2160,54,HAL_DATASPACE_BT2020_PQ)==HWC2_ERROR_UNSUPPORTED);
 CHECK(clientSupport(&d,0,3840,2160,1,HAL_DATASPACE_V0_SRGB)==0);
 CHECK(clientSupport(&d,0,8192,2160,43,HAL_DATASPACE_BT2020_PQ)==HWC2_ERROR_UNSUPPORTED);
 CHECK(clientSupport(&d,1,3840,2160,43,HAL_DATASPACE_BT2020_PQ)==HWC2_ERROR_BAD_DISPLAY);
 sinkHlg=false;n=6;CHECK(colorModes(&d,0,&n,out)==0&&n==3&&out[2]==11);
 CHECK(colorMode(&d,0,12)==HWC2_ERROR_BAD_PARAMETER);CHECK(clientSupport(&d,0,3840,2160,43,HAL_DATASPACE_BT2020_HLG)==HWC2_ERROR_UNSUPPORTED);
 sinkPq=false;n=6;CHECK(colorModes(&d,0,&n,out)==0&&n==2);
 CHECK(clientSupport(&d,0,3840,2160,43,HAL_DATASPACE_BT2020_PQ)==HWC2_ERROR_UNSUPPORTED);
 clientHdr10=false;CHECK(clientSupport(&d,0,3840,2160,43,HAL_DATASPACE_BT2020_PQ)==HWC2_ERROR_UNSUPPORTED);
 printf("PASS client_hdr_adapter cases=%u bounded_counts=1 sink_gating=1 no_P3_advertisement=1 no_8bit_HDR=1 dimensions_retained=1 default_off_passthrough=1 hardware_HDR_validation=NOT_RUN\n",checks);
 return 0;
}
