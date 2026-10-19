// SPDX-License-Identifier: MIT
// Runs the actual adapter commit hook against an isolated fake DRM interface.
// No display device is opened and no live property is changed.
#include <sys/ioctl.h>
extern "C" int testIoctl(int,unsigned long,...);
#define ioctl testIoctl
#define property_get_bool testPropertyBool
#include "../../magisk-hdr/src/hwc_bridge.cpp"
#undef ioctl
#undef property_get_bool
#include <cstdarg>
#include <cstdlib>

namespace {
Hdr liveHdr{};
bool reuse=true,enabled=true,createFail=false,addFail=false,commitFail=false,dupFail=false,blobFail=false,multi=false;
int creates=0,destroys=0,copies=0,originals=0,copyCommits=0;
uint32_t added=0;void *original=(void*)0x1000,*copied=(void*)0x2000;
int makeBlob(int,const void*,size_t,uint32_t *b){++creates;if(createFail)return -1;*b=99;return 0;}
int dropBlob(int,uint32_t b){if(b!=99)abort();++destroys;return 0;}
void *copyReq(const void *p){if(p!=original)abort();++copies;return dupFail?nullptr:copied;}
void freeCopy(void *p){if(p!=copied)abort();}
int addProp(void *p,uint32_t obj,uint32_t prop,uint64_t b){if(p!=copied||obj!=7||prop!=12)abort();added=b;return addFail?-1:0;}
int doCommit(int,void *p,uint32_t,void*){if(p==original){++originals;return 0;}if(p!=copied)abort();++copyCommits;return commitFail?-1:0;}
void reset(){
 liveHdr={};liveHdr.eotf=2;presented={};havePresented=false;presentedEotf=2;
 pending.clear();propertyKinds.clear();targetFd=-1;connector=hdrProperty=0;
 creates=destroys=copies=originals=copyCommits=0;added=0;
 reuse=enabled=true;createFail=addFail=commitFail=dupFail=blobFail=multi=false;
 realCreate=makeBlob;destroyBlob=dropBlob;duplicate=copyReq;freeReq=freeCopy;realAdd=addProp;realCommit=doCommit;
}
unsigned cases=0;
void check(bool v){if(!v){fprintf(stderr,"FAIL case %u added=%u creates=%d destroys=%d copies=%d copyCommits=%d originals=%d target=%u/%u\n",cases+1,added,creates,destroys,copies,copyCommits,originals,connector,hdrProperty);abort();}++cases;}
}
extern "C" int8_t testPropertyBool(const char *k,int8_t fallback){
 if(!strcmp(k,"debug.rkhdr.static"))return enabled;
 if(!strcmp(k,"debug.rkhdr.metadata_reuse"))return reuse;
 if(!strcmp(k,"debug.rkhdr.trace"))return false;
 return fallback;
}
extern "C" int testIoctl(int,unsigned long request,...){
 va_list a;va_start(a,request);void *arg=va_arg(a,void*);va_end(a);
 if(request==DRM_IOCTL_MODE_GETRESOURCES){auto r=(drm_mode_card_res*)arg;r->count_connectors=multi?2:1;
  if(r->connector_id_ptr){auto ids=(uint32_t*)(uintptr_t)r->connector_id_ptr;ids[0]=7;if(multi)ids[1]=8;}return 0;}
 if(request==DRM_IOCTL_MODE_OBJ_GETPROPERTIES){auto r=(drm_mode_obj_get_properties*)arg;r->count_props=2;
  if(r->props_ptr){auto ids=(uint32_t*)(uintptr_t)r->props_ptr;auto vals=(uint64_t*)(uintptr_t)r->prop_values_ptr;ids[0]=11;ids[1]=12;vals[0]=3;vals[1]=55;}return 0;}
 if(request==DRM_IOCTL_MODE_GETPROPERTY){auto p=(drm_mode_get_property*)arg;strcpy(p->name,p->prop_id==11?"CRTC_ID":"HDR_OUTPUT_METADATA");return 0;}
 if(request==DRM_IOCTL_MODE_GETPROPBLOB){if(blobFail)return -1;auto b=(drm_mode_get_blob*)arg;b->length=sizeof(Hdr);if(b->data)memcpy((void*)(uintptr_t)b->data,&liveHdr,sizeof(Hdr));return 0;}
 abort();
}
int main(){
 reset();check(!hookedCommit(4,original,0,nullptr)&&added==55&&creates==0&&destroys==0&&copyCommits==1);
 reset();reuse=false;check(!hookedCommit(4,original,0,nullptr)&&added==99&&creates==1&&destroys==1);
 reset();havePresented=true;presented.maximum=1000;check(!hookedCommit(4,original,0,nullptr)&&added==99&&creates==1&&destroys==1);
 reset();havePresented=true;presented.maximum=liveHdr.maximum=4000;check(!hookedCommit(4,original,0,nullptr)&&added==55&&creates==0);
 reset();liveHdr.eotf=3;presentedEotf=3;check(!hookedCommit(4,original,0,nullptr)&&added==55&&destroys==0);
 reset();liveHdr.eotf=0;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&copies==0);
 reset();pending[original].connectorEotf=3;check(!hookedCommit(4,original,0,nullptr)&&added==99&&creates==1&&pending.empty());
 reset();pending[original].connectorEotf=0;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&copies==0);
 reset();pending[original].connectorEotf=3;check(!hookedCommit(4,original,DRM_MODE_ATOMIC_TEST_ONLY,nullptr)&&pending.count(original)==1&&copies==0);
 reset();multi=true;check(!hookedCommit(4,original,0,nullptr)&&copies==0&&originals==1);
 reset();blobFail=true;check(!hookedCommit(4,original,0,nullptr)&&copies==0&&originals==1);
 reset();dupFail=true;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&creates==0);
 reset();addFail=true;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&destroys==0&&creates==0);
 reset();commitFail=true;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&copyCommits==1&&destroys==0);
 reset();reuse=false;createFail=true;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&destroys==0);
 reset();enabled=false;check(!hookedCommit(4,original,0,nullptr)&&originals==1&&copies==0);
 printf("PASS actual atomic hook: %u cases; no live DRM access\n",cases);return 0;
}
