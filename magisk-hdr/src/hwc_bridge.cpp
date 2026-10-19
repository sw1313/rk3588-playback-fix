// SPDX-License-Identifier: MIT
// Hash-gated adapter for the original HWC. Pixel buffers and the native plane
// planner are retained; HDR metadata joins the original atomic commit.
#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <ui/GraphicBufferMapper.h>
#include <cutils/properties.h>
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cerrno>
#include <map>
#include <mutex>
#include <optional>
#include <vector>
#include <algorithm>
#include "drm.h"
#include "drm_mode.h"
#include "transfer_policy.h"
#include "client_policy.h"
#include "mode_cache.h"
#ifdef RKQMS_HWC_CANDIDATE
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "qms_client.h"
#endif

namespace {
struct Hdr {
    uint32_t type;
    uint8_t eotf, descriptor;
    struct {uint16_t x,y;} primaries[3],white;
    uint16_t maximum,minimum,cll,fall;
};
static_assert(sizeof(Hdr)==32,"original firmware HDR blob ABI");
struct Layer {Hdr hdr{};int32_t space=0,requested=HWC2_COMPOSITION_INVALID,composition=HWC2_COMPOSITION_INVALID;int64_t area=0;bool metadata=false;};
std::mutex mutex;
std::map<std::pair<uint64_t,uint64_t>,Layer> layers;
struct ClientTarget {bool nativeSdr=false;int hdrEotf=-1;};
std::map<uint64_t,ClientTarget> clientTargets;
struct Transaction {int connectorEotf=-1;std::map<uint32_t,rk_hdr::Plane> planes;};
std::map<void*,Transaction> pending;
std::map<uint32_t,int> propertyKinds;
Hdr presented{};bool havePresented=false;
int presentedEotf=-1;
hwc2_function_pointer_t (*originalGet)(hwc2_device_t*,int32_t)=nullptr;
struct Validation {hwc2_device_t *device=nullptr;hwc2_display_t display=0;};
thread_local Validation validation;
thread_local bool clientOutputSdr=false;
thread_local bool importingHdrTarget=false;
int (*realSwitchHdr)(void*)=nullptr;
int (*realPropertyGet)(const char*,char*,const char*)=nullptr;
using Add=int(*)(void*,uint32_t,uint32_t,uint64_t);
using Commit=int(*)(int,void*,uint32_t,void*);
using Create=int(*)(int,const void*,size_t,uint32_t*);
Add realAdd=nullptr;Commit realCommit=nullptr;Create realCreate=nullptr;
void *(*duplicate)(const void*)=nullptr;void (*freeReq)(void*)=nullptr;
int (*destroyBlob)(int,uint32_t)=nullptr;
int targetFd=-1;uint32_t connector=0,hdrProperty=0;
bool clientHdr10=false;
int clientColorMode=HAL_COLOR_MODE_NATIVE;
bool realSfTargetReady() {
    char ready[PROPERTY_VALUE_MAX]{};property_get("vendor.rkhdr.sf_target10",ready,"");
    if(strcmp(ready,"active"))return false;
    property_get("vendor.rkhdr.alloc10",ready,"");return !strcmp(ready,"active");
}
bool (*sfTargetReady)()=realSfTargetReady;
bool clientHdrActive(){return clientHdr10 && sfTargetReady();}
bool targetPlanFailed=false;
bool primaryHdrPlan(hwc2_display_t display){
    int mode=__atomic_load_n(&clientColorMode,__ATOMIC_ACQUIRE);
    return display==0 && clientHdrActive() &&
        (mode==HAL_COLOR_MODE_BT2100_PQ || mode==HAL_COLOR_MODE_BT2100_HLG);
}
void reportTargetPlanFailure(){property_set("vendor.rkhdr.target_plan_error","1");}
void (*targetPlanFailure)()=reportTargetPlanFailure;

std::vector<int32_t> clientColorModes(hwc2_device_t *d,hwc2_display_t display) {
    std::vector<int32_t> modes{HAL_COLOR_MODE_NATIVE,HAL_COLOR_MODE_SRGB};
    auto fn=reinterpret_cast<HWC2_PFN_GET_HDR_CAPABILITIES>(originalGet(d,HWC2_FUNCTION_GET_HDR_CAPABILITIES));
    if(!fn)return {HAL_COLOR_MODE_NATIVE};
    uint32_t n=0;float max=0,average=0,min=0;
    if(fn(d,display,&n,nullptr,&max,&average,&min)!=HWC2_ERROR_NONE || n>32)return {HAL_COLOR_MODE_NATIVE};
    std::vector<int32_t> types(n);uint32_t capacity=n;
    if(n && (fn(d,display,&n,types.data(),&max,&average,&min)!=HWC2_ERROR_NONE || n>capacity))return {HAL_COLOR_MODE_NATIVE};
    bool pq=false,hlg=false;for(uint32_t i=0;i<n;i++){pq|=types[i]==2;hlg|=types[i]==3;}
    if(pq)modes.push_back(HAL_COLOR_MODE_BT2100_PQ);
    if(hlg)modes.push_back(HAL_COLOR_MODE_BT2100_HLG);
    return modes;
}
int32_t colorModes(hwc2_device_t *d,hwc2_display_t display,uint32_t *count,int32_t *out) {
    auto original=reinterpret_cast<HWC2_PFN_GET_COLOR_MODES>(originalGet(d,HWC2_FUNCTION_GET_COLOR_MODES));
    if(!clientHdrActive() || display!=0)return original(d,display,count,out);
    if(!count)return HWC2_ERROR_BAD_PARAMETER;
    auto modes=clientColorModes(d,display);
    uint32_t written=out?std::min(*count,uint32_t(modes.size())):uint32_t(modes.size());
    if(out)for(uint32_t i=0;i<written;i++)out[i]=modes[i];*count=written;return HWC2_ERROR_NONE;
}
int32_t colorMode(hwc2_device_t *d,hwc2_display_t display,int32_t mode) {
    auto original=reinterpret_cast<HWC2_PFN_SET_COLOR_MODE>(originalGet(d,HWC2_FUNCTION_SET_COLOR_MODE));
    if(!clientHdrActive() || display!=0)return original(d,display,mode);
    auto modes=clientColorModes(d,display);bool supported=false;for(auto m:modes)supported|=m==mode;
    if(!supported)return HWC2_ERROR_BAD_PARAMETER;
    // The factory's SetColorMode only stores NATIVE. Its actual plane color
    // conversion and transfer selection use SetClientTarget's real dataspace.
    // Do not modify source-layer metadata or pretend an SDR target is HDR.
    int32_t rc=original(d,display,HAL_COLOR_MODE_NATIVE);
    if(rc==HWC2_ERROR_NONE)__atomic_store_n(&clientColorMode,mode,__ATOMIC_RELEASE);
    __android_log_print(ANDROID_LOG_INFO,"RKClientHdr","mode=%d result=%d",mode,rc);return rc;
}
int32_t clientSupport(hwc2_device_t *d,hwc2_display_t display,uint32_t width,uint32_t height,int32_t format,int32_t space) {
    auto original=reinterpret_cast<HWC2_PFN_GET_CLIENT_TARGET_SUPPORT>(originalGet(d,HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT));
    if(!clientHdrActive() || display!=0)return original(d,display,width,height,format,space);
    int mode=space==HAL_DATASPACE_BT2020_PQ?HAL_COLOR_MODE_BT2100_PQ:space==HAL_DATASPACE_BT2020_HLG?HAL_COLOR_MODE_BT2100_HLG:space==HAL_DATASPACE_V0_SRGB?HAL_COLOR_MODE_SRGB:-1;
    if(mode<0)return original(d,display,width,height,format,space);
    // Standard RGBA_1010102 only for HDR. No P010-as-RGB alias, eight-bit HDR,
    // DISPLAY_P3 or opaque vendor layout is advertised by this candidate.
    if(format!=43 && !(mode==HAL_COLOR_MODE_SRGB && format==1))return HWC2_ERROR_UNSUPPORTED;
    auto modes=clientColorModes(d,display);bool supported=false;for(auto m:modes)supported|=m==mode;
    return supported?original(d,display,width,height,format,HAL_DATASPACE_UNKNOWN):HWC2_ERROR_UNSUPPORTED;
}

uint16_t quantize(float value,double scale) {
    if(!std::isfinite(value) || value<=0)return 0;
    return uint16_t(std::fmin(65535.0,std::round(double(value)*scale)));
}
bool hdrBlob(int fd,uint32_t id,Hdr *hdr) {
    if(!id)return false;
    drm_mode_get_blob b{};b.blob_id=id;
    if(ioctl(fd,DRM_IOCTL_MODE_GETPROPBLOB,&b) || b.length!=sizeof(Hdr))return false;
    b.data=reinterpret_cast<uintptr_t>(hdr);
    return !ioctl(fd,DRM_IOCTL_MODE_GETPROPBLOB,&b) && hdr->type==0 && hdr->eotf<=5;
}
bool objectProperties(int fd,uint32_t obj,uint32_t *prop,uint32_t *blob,bool *active) {
    drm_mode_obj_get_properties o{};o.obj_id=obj;o.obj_type=DRM_MODE_OBJECT_CONNECTOR;
    if(ioctl(fd,DRM_IOCTL_MODE_OBJ_GETPROPERTIES,&o) || o.count_props>256)return false;
    std::vector<uint32_t> ids(o.count_props);std::vector<uint64_t> values(o.count_props);
    const size_t capacity=ids.size();o.props_ptr=(uintptr_t)ids.data();o.prop_values_ptr=(uintptr_t)values.data();
    if(ioctl(fd,DRM_IOCTL_MODE_OBJ_GETPROPERTIES,&o))return false;
    *active=false;*prop=0;*blob=0;
    for(size_t i=0;i<capacity && i<o.count_props;++i){
        drm_mode_get_property p{};p.prop_id=ids[i];
        if(ioctl(fd,DRM_IOCTL_MODE_GETPROPERTY,&p))continue;
        p.name[sizeof(p.name)-1]=0;
        if(!strcmp(p.name,"CRTC_ID"))*active=values[i]!=0;
        if(!strcmp(p.name,"HDR_OUTPUT_METADATA")){*prop=ids[i];*blob=uint32_t(values[i]);}
    }
    return *prop!=0;
}
bool target(int fd,uint32_t *blob) {
    bool active=false;uint32_t p=0;
    connector=hdrProperty=0;targetFd=fd;
    drm_mode_card_res r{};
    if(ioctl(fd,DRM_IOCTL_MODE_GETRESOURCES,&r) || r.count_connectors>32 || r.count_crtcs>32 || r.count_encoders>64 || r.count_fbs>4096)return false;
    std::vector<uint32_t> c(r.count_connectors),crtcs(r.count_crtcs),e(r.count_encoders),f(r.count_fbs);
    size_t capacity=c.size();r.connector_id_ptr=(uintptr_t)c.data();r.crtc_id_ptr=(uintptr_t)crtcs.data();
    r.encoder_id_ptr=(uintptr_t)e.data();r.fb_id_ptr=(uintptr_t)f.data();
    if(ioctl(fd,DRM_IOCTL_MODE_GETRESOURCES,&r))return false;
    unsigned activeCount=0;uint32_t selectedBlob=0;
    for(size_t i=0;i<capacity && i<r.count_connectors;++i) {
        uint32_t candidateBlob=0;
        bool hasHdr=objectProperties(fd,c[i],&p,&candidateBlob,&active);
        if(active)++activeCount;
        if(hasHdr && active){connector=c[i];hdrProperty=p;selectedBlob=candidateBlob;}
    }
    // This adapter is validated on the primary single-output path. Never apply
    // a primary layer's metadata to another active connector's transaction.
    if(activeCount!=1 || !connector){connector=hdrProperty=0;return false;}
    *blob=selectedBlob;return true;
}

int hookedAdd(void *req,uint32_t obj,uint32_t prop,uint64_t value) {
    int result=realAdd(req,obj,prop,value);
    if(result<0)return result;
    // The target property is resolved at the first commit. Subsequent original
    // HDR transitions can be identified before their atomic transaction runs.
    {std::lock_guard<std::mutex> lock(mutex);
        if(obj==connector && prop==hdrProperty && targetFd>=0){
            Hdr h{};if(!value || hdrBlob(targetFd,uint32_t(value),&h)){
                if(pending.size()>128)pending.clear();pending[req].connectorEotf=h.eotf;
            }
        }
        if(targetFd>=0 && obj!=connector){
            auto kind=propertyKinds.find(prop);
            if(kind==propertyKinds.end()){
                drm_mode_get_property p{};p.prop_id=prop;
                if(!ioctl(targetFd,DRM_IOCTL_MODE_GETPROPERTY,&p)){
                    p.name[sizeof(p.name)-1]=0;
                    int k=!strcmp(p.name,"EOTF")?1:!strcmp(p.name,"FB_ID")?2:!strcmp(p.name,"CRTC_ID")?3:0;
                    kind=propertyKinds.emplace(prop,k).first;
                }
            }
            if(kind!=propertyKinds.end() && kind->second){
                if(pending.size()>128)pending.clear();
                auto &plane=pending[req].planes[obj];
                if(kind->second==1)plane.eotf=value<=5?int(value):-1;
                if(kind->second==2)plane.fb=value;
                if(kind->second==3)plane.crtc=value;
            }
        }
    }
    return result;
}
void hookedFree(void *req) {
    {std::lock_guard<std::mutex> lock(mutex);pending.erase(req);}
    freeReq(req);
}
int hookedCommit(int fd,void *req,uint32_t flags,void *data) {
    Hdr h{};bool inject=false;uint32_t obj=0,prop=0,reuseBlob=0;
    {
        std::lock_guard<std::mutex> lock(mutex);uint32_t blob=0;
        auto transition=pending.find(req);
        if(property_get_bool("debug.rkhdr.static",false) && !(flags & DRM_MODE_ATOMIC_TEST_ONLY)
                && target(fd,&blob) && hdrBlob(fd,blob,&h)) {
            const Hdr live=h;
            if(transition!=pending.end() && transition->second.connectorEotf>=0)
                h.eotf=transition->second.connectorEotf;
            // The stock display/compositor cache only HDR enabled/disabled,
            // leaving PQ/HLG stale on a reused Surface. The actual plane EOTF
            // is still added per frame. Match that same transaction, only
            // while the original output is already HDR. Never enable HDR from
            // SDR or override force-disable/area policy or mixed HDR planes.
            if(h.eotf==2 || h.eotf==3){
                // This exact original HWC maps PQ YUV planes to 2 and HLG
                // planes to 0. Correlate that with the validated DEVICE HDR
                // layer, rather than mistaking every SDR UI plane for HLG.
                if(transition!=pending.end())
                    h.eotf=uint8_t(rk_hdr::outputTransfer(h.eotf,presentedEotf,transition->second.planes));
                if(property_get_bool("debug.rkhdr.trace",false)){
                    static int previous=-999;int signature=presentedEotf*10+h.eotf;
                    if(signature!=previous){previous=signature;
                        __android_log_print(ANDROID_LOG_INFO,"RKHdrHwc","HDR transaction device_eotf=%d output_eotf=%u planes=%zu",
                                presentedEotf,h.eotf,transition==pending.end()?0:transition->second.planes.size());}
                }
                uint8_t eotf=h.eotf;h={};h.eotf=eotf;
                if(eotf==2 && havePresented){h=presented;h.eotf=eotf;}
                inject=true;obj=connector;prop=hdrProperty;
                // Borrow only the freshly read live blob. Never retain a blob
                // ID, connector or fd across commits: hotplug, SDR transitions
                // and other writers must be observed again on the next frame.
                // Still override the property in the duplicate request, since
                // the original request may itself contain different metadata.
                if(property_get_bool("debug.rkhdr.metadata_reuse",false)) {
                    if(!memcmp(&live,&h,sizeof(h)))reuseBlob=blob;
                }
            }
        }
        // A successful TEST_ONLY request may be committed again unchanged.
        // Retain its EOTF until real commit or the original request is freed.
        if(!(flags & DRM_MODE_ATOMIC_TEST_ONLY) && transition!=pending.end())pending.erase(transition);
    }
    if(!inject)return realCommit(fd,req,flags,data);
    uint32_t blob=reuseBlob;
    void *copy=duplicate(req);
    if(!copy)return realCommit(fd,req,flags,data);
    int rc=-1;
    if((reuseBlob || !realCreate(fd,&h,sizeof(h),&blob)) && realAdd(copy,obj,prop,blob)>=0)
        rc=realCommit(fd,copy,flags,data);
    // A live blob is owned by the original creator/kernel state, not by us.
    if(blob && !reuseBlob)destroyBlob(fd,blob);freeReq(copy);
    if(property_get_bool("debug.rkhdr.trace",false)) {
        static unsigned commits=0,reused=0,created=0,failed=0;
        unsigned n=__atomic_add_fetch(&commits,1,__ATOMIC_RELAXED);
        if(reuseBlob)__atomic_add_fetch(&reused,1,__ATOMIC_RELAXED);
        else if(blob)__atomic_add_fetch(&created,1,__ATOMIC_RELAXED);
        if(rc)__atomic_add_fetch(&failed,1,__ATOMIC_RELAXED);
        if(n%120==0)__android_log_print(ANDROID_LOG_INFO,"RKHdrHwc",
            "metadata commits=%u reused=%u created=%u rejected=%u",n,
            __atomic_load_n(&reused,__ATOMIC_RELAXED),__atomic_load_n(&created,__ATOMIC_RELAXED),
            __atomic_load_n(&failed,__ATOMIC_RELAXED));
    }
    // A rejected atomic transaction made no display change. Retry the untouched
    // original request to preserve playback even if metadata injection fails.
    if(rc){static unsigned rejected=0;
        if(__atomic_add_fetch(&rejected,1,__ATOMIC_RELAXED)%256==1)
            __android_log_print(ANDROID_LOG_WARN,"RKHdrHwc","metadata commit rejected; original request retained");
        return realCommit(fd,req,flags,data);}
    return rc;
}

int32_t setBuffer(hwc2_device_t *d,hwc2_display_t display,hwc2_layer_t layer,buffer_handle_t b,int32_t fence) {
    auto fn=reinterpret_cast<HWC2_PFN_SET_LAYER_BUFFER>(originalGet(d,HWC2_FUNCTION_SET_LAYER_BUFFER));
    int32_t result=fn(d,display,layer,b,fence);
    if(result!=HWC2_ERROR_NONE)return result;
    Layer metadata;
    if(b && property_get_bool("debug.rkhdr.static",false)) {
        auto &mapper=android::GraphicBufferMapper::get();
        std::optional<android::ui::Smpte2086> mastering;
        std::optional<android::ui::Cta861_3> light;
        if(mapper.getSmpte2086(b,&mastering)==android::OK && mastering && mastering->maxLuminance>0) {
            auto &m=*mastering;metadata.metadata=true;
            metadata.hdr.primaries[0]={quantize(m.primaryRed.x,50000),quantize(m.primaryRed.y,50000)};
            metadata.hdr.primaries[1]={quantize(m.primaryGreen.x,50000),quantize(m.primaryGreen.y,50000)};
            metadata.hdr.primaries[2]={quantize(m.primaryBlue.x,50000),quantize(m.primaryBlue.y,50000)};
            metadata.hdr.white={quantize(m.whitePoint.x,50000),quantize(m.whitePoint.y,50000)};
            metadata.hdr.maximum=quantize(m.maxLuminance,1);metadata.hdr.minimum=quantize(m.minLuminance,10000);
        }
        if(mapper.getCta861_3(b,&light)==android::OK && light) {
            metadata.hdr.cll=quantize(light->maxContentLightLevel,1);metadata.hdr.fall=quantize(light->maxFrameAverageLightLevel,1);
            metadata.metadata|=metadata.hdr.cll || metadata.hdr.fall;
        }
    }
    std::lock_guard<std::mutex> lock(mutex);auto &state=layers[{display,layer}];
    state.hdr=metadata.hdr;state.metadata=metadata.metadata;return result;
}
int32_t setSpace(hwc2_device_t *d,hwc2_display_t display,hwc2_layer_t layer,int32_t space) {
    auto fn=reinterpret_cast<HWC2_PFN_SET_LAYER_DATASPACE>(originalGet(d,HWC2_FUNCTION_SET_LAYER_DATASPACE));
    auto rc=fn(d,display,layer,space);
    if(rc==HWC2_ERROR_NONE){std::lock_guard<std::mutex> lock(mutex);layers[{display,layer}].space=space;}
    return rc;
}
int32_t setFrame(hwc2_device_t *d,hwc2_display_t display,hwc2_layer_t layer,hwc_rect_t frame) {
    auto fn=reinterpret_cast<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(originalGet(d,HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME));
    auto rc=fn(d,display,layer,frame);
    if(rc==HWC2_ERROR_NONE){std::lock_guard<std::mutex> lock(mutex);layers[{display,layer}].area=int64_t(frame.right-frame.left)*(frame.bottom-frame.top);}
    return rc;
}
int32_t destroyLayer(hwc2_device_t *d,hwc2_display_t display,hwc2_layer_t layer) {
    auto fn=reinterpret_cast<HWC2_PFN_DESTROY_LAYER>(originalGet(d,HWC2_FUNCTION_DESTROY_LAYER));
    auto rc=fn(d,display,layer);
    if(rc==HWC2_ERROR_NONE){std::lock_guard<std::mutex> lock(mutex);layers.erase({display,layer});}
    return rc;
}
int32_t setType(hwc2_device_t *d,hwc2_display_t display,hwc2_layer_t layer,int32_t type) {
    auto fn=reinterpret_cast<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(originalGet(d,HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE));
    int32_t rc=fn(d,display,layer,type);
    if(rc==HWC2_ERROR_NONE){std::lock_guard<std::mutex> lock(mutex);layers[{display,layer}].requested=type;}
    return rc;
}
bool readComposition(hwc2_device_t *d,hwc2_display_t display,int32_t rc) {
    auto changed=reinterpret_cast<HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES>(originalGet(d,HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES));
    std::vector<hwc2_layer_t> ids;std::vector<int32_t> compositions;bool known=false;
    if((rc==HWC2_ERROR_NONE || rc==HWC2_ERROR_HAS_CHANGES) && changed){
        uint32_t count=0;
        if(changed(d,display,&count,nullptr,nullptr)==HWC2_ERROR_NONE && count<=256){
            ids.resize(count);compositions.resize(count);uint32_t capacity=count;
            known=!count || changed(d,display,&count,ids.data(),compositions.data())==HWC2_ERROR_NONE;
            if(count>capacity)known=false;else {ids.resize(count);compositions.resize(count);}
        }
    }
    std::lock_guard<std::mutex> lock(mutex);
    for(auto &item:layers)if(item.first.first==display)
        item.second.composition=known?item.second.requested:HWC2_COMPOSITION_INVALID;
    if(known)for(size_t i=0;i<ids.size();++i){auto it=layers.find({display,ids[i]});if(it!=layers.end())it->second.composition=compositions[i];}
    return known;
}
int32_t setClientTarget(hwc2_device_t *d,hwc2_display_t display,buffer_handle_t b,int32_t fence,int32_t space,hwc_region_t damage) {
    auto fn=reinterpret_cast<HWC2_PFN_SET_CLIENT_TARGET>(originalGet(d,HWC2_FUNCTION_SET_CLIENT_TARGET));
    int32_t rc=fn(d,display,b,fence,space,damage);
    if(rc!=HWC2_ERROR_NONE)return rc;
    uint32_t fourcc=0;
    // On the hash-pinned managed API32 compositor, UNKNOWN on its native
    // eight-bit AB24 client target is rendered as sRGB, including HDR tone
    // mapping. This is output metadata, not the source video's dataspace.
    bool known=b && android::GraphicBufferMapper::get().getPixelFormatFourCC(b,&fourcc)==android::OK;
    std::lock_guard<std::mutex> lock(mutex);
    bool nativeSdr=rk_hdr::nextClientTarget(clientTargets[display].nativeSdr,b!=nullptr,known,fourcc,space);
    if(property_get_bool("debug.rkhdr.trace",false)){
        static int previous=-1;int signature=space+int(fourcc)+nativeSdr;
        if(previous!=signature){previous=signature;
            __android_log_print(ANDROID_LOG_INFO,"RKHdrHwc","client target display=%llu space=%x fourcc=%x native_sdr=%d",
                    (unsigned long long)display,space,fourcc,nativeSdr);}
    }
    clientTargets[display].nativeSdr=nativeSdr;
    if(b)clientTargets[display].hdrEotf=clientHdrActive() && known &&
        (fourcc==0x30334241 || fourcc==0x30335241 || fourcc==0x30334258 || fourcc==0x30335258)?
        (space==HAL_DATASPACE_BT2020_PQ?2:space==HAL_DATASPACE_BT2020_HLG?3:-1):-1;
    return rc;
}
int32_t accept(hwc2_device_t *d,hwc2_display_t display) {
    auto fn=reinterpret_cast<HWC2_PFN_ACCEPT_DISPLAY_CHANGES>(originalGet(d,HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES));
    int32_t rc=fn(d,display);
    if(rc==HWC2_ERROR_NONE){std::lock_guard<std::mutex> lock(mutex);
        for(auto &item:layers)if(item.first.first==display && item.second.composition!=HWC2_COMPOSITION_INVALID)
            item.second.requested=item.second.composition;
    }
    return rc;
}
int hookedPropertyGet(const char *key,char *value,const char *fallback) {
    if(value && rk_hdr::overrideForceDisable(clientOutputSdr,key)){
        value[0]='1';value[1]=0;return 1;
    }
    return realPropertyGet(key,value,fallback);
}
int hookedSwitchHdr(void *displayObject) {
    bool useSdr=false;
    if(validation.device && validation.display==0 && property_get_bool("debug.rkhdr.static",false) &&
            property_get_bool("ro.surface_flinger.use_color_management",true) &&
            property_get_bool("debug.rkhdr.client_sdr",true)){
        bool known=readComposition(validation.device,validation.display,HWC2_ERROR_HAS_CHANGES);
        std::lock_guard<std::mutex> lock(mutex);uint32_t blob=0;
        bool clientHdr=false,deviceHdr=false,unknownHdr=false;
        for(const auto &item:layers)if(item.first.first==validation.display && item.second.area>0){
            auto t=uint32_t(item.second.space)&HAL_DATASPACE_TRANSFER_MASK;
            if(t==HAL_DATASPACE_TRANSFER_ST2084 || t==HAL_DATASPACE_TRANSFER_HLG){
                clientHdr|=item.second.composition==HWC2_COMPOSITION_CLIENT;
                deviceHdr|=item.second.composition==HWC2_COMPOSITION_DEVICE;
                unknownHdr|=item.second.composition!=HWC2_COMPOSITION_CLIENT && item.second.composition!=HWC2_COMPOSITION_DEVICE;
            }
        }
        bool singlePrimary=targetFd>=0 && target(targetFd,&blob);
        useSdr=rk_hdr::clientSdrRoute(known,clientTargets[validation.display].nativeSdr,singlePrimary,
                                    clientHdr,deviceHdr,unknownHdr);
    }
    // Invoke the original policy exactly once, after its real plane planner.
    // Only its scoped force-disable read is overridden for an already SDR
    // client target. No global property, layer dataspace or pixel is changed.
    bool saved=clientOutputSdr;clientOutputSdr=useSdr;
    int rc=realSwitchHdr(displayObject);clientOutputSdr=saved;
    if(property_get_bool("debug.rkhdr.trace",false)){
        static int previous=-1;if(previous!=int(useSdr)){previous=useSdr;
            __android_log_print(ANDROID_LOG_INFO,"RKHdrHwc","native client SDR policy active=%d",useSdr);}
    }
    return rc;
}
int32_t validate(hwc2_device_t *d,hwc2_display_t display,uint32_t *types,uint32_t *requests) {
    auto fn=reinterpret_cast<HWC2_PFN_VALIDATE_DISPLAY>(originalGet(d,HWC2_FUNCTION_VALIDATE_DISPLAY));
    auto saved=validation;validation={d,display};
    int32_t rc=fn(d,display,types,requests);validation=saved;
    if(primaryHdrPlan(display) && rc!=HWC2_ERROR_NONE && rc!=HWC2_ERROR_HAS_CHANGES){
        __atomic_store_n(&targetPlanFailed,true,__ATOMIC_RELEASE);targetPlanFailure();
    }
    readComposition(d,display,rc);return rc;
}
int32_t present(hwc2_device_t *d,hwc2_display_t display,int32_t *fence) {
    // A failed HDR validation has no plane plan or release-fence set. Enforce
    // HWC2's NOT_VALIDATED error instead of entering the factory's null fence
    // path. Do not claim a present, fabricate a fence or report success; the
    // boot service removes the paired candidate by reboot rollback.
    if(display==0 && __atomic_load_n(&targetPlanFailed,__ATOMIC_ACQUIRE))return HWC2_ERROR_NOT_VALIDATED;
    if(display==0) {
        std::lock_guard<std::mutex> lock(mutex);havePresented=false;presented={};presentedEotf=-1;int64_t area=-1;bool mixed=false;
        for(auto &item:layers){const auto &layer=item.second;
            auto t=uint32_t(layer.space)&HAL_DATASPACE_TRANSFER_MASK;
            if(item.first.first==display && layer.area>0 && layer.composition==HWC2_COMPOSITION_DEVICE &&
                    (t==HAL_DATASPACE_TRANSFER_ST2084 || t==HAL_DATASPACE_TRANSFER_HLG)){
                int e=t==HAL_DATASPACE_TRANSFER_ST2084?2:3;
                if(presentedEotf>=0 && presentedEotf!=e)mixed=true;presentedEotf=e;
            }
            if(item.first.first==display && (uint32_t(layer.space)&HAL_DATASPACE_TRANSFER_MASK)==HAL_DATASPACE_TRANSFER_ST2084 && layer.metadata && layer.area>area){
                area=layer.area;presented=layer.hdr;havePresented=true;
            }
        }
        if(mixed)presentedEotf=-1;
        if(!mixed && clientHdrActive() && clientTargets[display].hdrEotf>=0) {
            int client=clientTargets[display].hdrEotf;
            if(presentedEotf<0 || presentedEotf==client)presentedEotf=client;
            else presentedEotf=-1;
        }
    }
    auto fn=reinterpret_cast<HWC2_PFN_PRESENT_DISPLAY>(originalGet(d,HWC2_FUNCTION_PRESENT_DISPLAY));
    bool savedImport=importingHdrTarget;importingHdrTarget=primaryHdrPlan(display);
    int32_t rc=fn(d,display,fence);importingHdrTarget=savedImport;return rc;
}
#ifdef RKQMS_HWC_CANDIDATE
#include "qms_frontend.h"
#endif
hwc2_function_pointer_t getFunction(hwc2_device_t *d,int32_t descriptor) {
    auto original=originalGet(d,descriptor);if(!original)return nullptr;
    switch(descriptor){
#ifdef RKQMS_HWC_CANDIDATE
    case HWC2_FUNCTION_SET_ACTIVE_CONFIG:return reinterpret_cast<hwc2_function_pointer_t>(qmsSetConfig);
    case HWC2_FUNCTION_GET_ACTIVE_CONFIG:return reinterpret_cast<hwc2_function_pointer_t>(qmsGetConfig);
    case HWC2_FUNCTION_SET_POWER_MODE:return reinterpret_cast<hwc2_function_pointer_t>(qmsPower);
#endif
    case HWC2_FUNCTION_GET_COLOR_MODES:return reinterpret_cast<hwc2_function_pointer_t>(colorModes);
    case HWC2_FUNCTION_SET_COLOR_MODE:return reinterpret_cast<hwc2_function_pointer_t>(colorMode);
    case HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT:return reinterpret_cast<hwc2_function_pointer_t>(clientSupport);
    case HWC2_FUNCTION_SET_LAYER_BUFFER:return reinterpret_cast<hwc2_function_pointer_t>(setBuffer);
    case HWC2_FUNCTION_SET_LAYER_DATASPACE:return reinterpret_cast<hwc2_function_pointer_t>(setSpace);
    case HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME:return reinterpret_cast<hwc2_function_pointer_t>(setFrame);
    case HWC2_FUNCTION_DESTROY_LAYER:return reinterpret_cast<hwc2_function_pointer_t>(destroyLayer);
    case HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE:return reinterpret_cast<hwc2_function_pointer_t>(setType);
    case HWC2_FUNCTION_VALIDATE_DISPLAY:return reinterpret_cast<hwc2_function_pointer_t>(validate);
    case HWC2_FUNCTION_SET_CLIENT_TARGET:return reinterpret_cast<hwc2_function_pointer_t>(setClientTarget);
    case HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES:return reinterpret_cast<hwc2_function_pointer_t>(accept);
    case HWC2_FUNCTION_PRESENT_DISPLAY:return reinterpret_cast<hwc2_function_pointer_t>(present);
    default:return original;
    }
}

struct Slot {void **pointer;void *saved;int protection;};
bool writeSlot(const Slot&,void*);
int protections(void*);
using PopulateFb=void(*)(void*,uint64_t,void*,void*,uint32_t,bool);
using MapperSelect=uint64_t(*)(uint64_t,int,uint64_t,int);
PopulateFb originalPopulateFb=nullptr;
MapperSelect originalMapperSelect=nullptr;
int (*originalLayerInit)(void*)=nullptr;
void* primaryFbSelf=nullptr;
template<class T> void fbStore(void* layer,size_t offset,T value){memcpy(static_cast<char*>(layer)+offset,&value,sizeof(value));}
template<class T> T fbLoad(void* layer,size_t offset){T value;memcpy(&value,static_cast<char*>(layer)+offset,sizeof(value));return value;}
void populateFb(void* self,uint64_t id,void* layer,void* ctx,uint32_t frame,bool preview){
    originalPopulateFb(self,id,layer,ctx,frame,preview);
    if(preview && validation.device && validation.display==0)__atomic_store_n(&primaryFbSelf,self,__ATOMIC_RELEASE);
    if(!clientHdrActive() || self!=__atomic_load_n(&primaryFbSelf,__ATOMIC_ACQUIRE))return;
    int mode=__atomic_load_n(&clientColorMode,__ATOMIC_ACQUIRE);
    if(mode!=HAL_COLOR_MODE_BT2100_PQ && mode!=HAL_COLOR_MODE_BT2100_HLG)return;
    // Validation precedes the next client buffer. Its factory placeholder is
    // AB24/AFBC. Predict the paired allocator's real AB30/AFBC instead.
    // Predict only this paired physical HDR target, and verify real metadata
    // again at present. Other targets and all original fences remain intact.
    if(!preview && (fbLoad<int32_t>(layer,0x124)!=43 || fbLoad<uint32_t>(layer,0x140)!=0x30334241U || fbLoad<uint64_t>(layer,0x148)!=0x0800000000000001ULL))return;
    fbStore<int32_t>(layer,0x124,43);
    fbStore<uint32_t>(layer,0x140,0x30334241U);
    fbStore<uint64_t>(layer,0x148,0x0800000000000001ULL);
    fbStore<int32_t>(layer,0x17c,mode==HAL_COLOR_MODE_BT2100_PQ?HAL_DATASPACE_BT2020_PQ:HAL_DATASPACE_BT2020_HLG);
    originalLayerInit(layer);
    // Keep factory AFBC planning and fences. RGB PQ additionally needs the
    // actual input EOTF; factory Init only marks PQ for YUV formats.
    fbStore<uint16_t>(layer,0x184,mode==HAL_COLOR_MODE_BT2100_PQ?2:0);
}
bool prepareTargetPlan(void* module,Slot* slot){
    Dl_info info{};if(!dladdr(module,&info))return false;
    uintptr_t base=uintptr_t(info.dli_fbase);
    void* lib=dlopen(info.dli_fname,RTLD_NOW|RTLD_LOCAL|RTLD_NOLOAD);if(!lib)return false;
    auto populate=reinterpret_cast<PopulateFb>(dlsym(lib,"_ZN7android9DrmHwcTwo8HwcLayer10PopulateFBEmPNS_11DrmHwcLayerEP16hwc2_drm_displayjb"));
    auto init=reinterpret_cast<int(*)(void*)>(dlsym(lib,"_ZN7android11DrmHwcLayer4InitEv"));dlclose(lib);
    if(uintptr_t(populate)!=base+0x3bffc || uintptr_t(init)!=base+0x6abac)return false;
    slot->pointer=reinterpret_cast<void**>(base+0x93748);slot->saved=*slot->pointer;slot->protection=protections(slot->pointer);
    if(slot->saved!=reinterpret_cast<void*>(populate) || slot->protection<0)return false;
    originalPopulateFb=populate;originalLayerInit=init;return true;
}
uint64_t mapperSelect(uint64_t format,int type,uint64_t usage,int area){
    uint64_t result=originalMapperSelect(format,type,usage,area);
    // Mapper recomputes allocation layout while validateBufferSize imports
    // the real FB. Match the paired allocator's format choice only during the
    // primary HDR present. Keep the full original size/stride/plane checks.
    if(importingHdrTarget && format==43 && (usage&0x1000) &&
       (result==1 || result==0x100000001ULL))return (result&0xffffffff00000000ULL)|43;
    return result;
}
struct MapperImage {uintptr_t base=0;unsigned count=0;};
int findMapperImage(dl_phdr_info* image,size_t,void* opaque){
    if(image->dlpi_name && !strcmp(image->dlpi_name,"/vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl-bifrost.so")){
        auto found=static_cast<MapperImage*>(opaque);found->base=image->dlpi_addr;++found->count;
    }
    return 0;
}
bool prepareMapperSelect(Slot* slot){
    // Use libui's already-loaded SP-HAL image. A direct dlopen can create a
    // separate mapper in the caller namespace and leave validation untouched.
    if(android::GraphicBufferMapper::get().getMapperVersion()!=android::GraphicBufferMapper::GRALLOC_4)return false;
    MapperImage found{};dl_iterate_phdr(findMapperImage,&found);
    if(found.count!=1)return false;
    uintptr_t base=found.base;void* select=reinterpret_cast<void*>(base+0x1b568);
    slot->pointer=reinterpret_cast<void**>(base+0x1fc58);slot->saved=*slot->pointer;slot->protection=protections(slot->pointer);
    if(slot->saved!=select || slot->protection<0)return false;
    originalMapperSelect=reinterpret_cast<MapperSelect>(select);return true;
}
bool installTargetPlan(void* module){
    Slot slot{},mapper{};if(!prepareTargetPlan(module,&slot) || !prepareMapperSelect(&mapper))return false;
    if(writeSlot(slot,reinterpret_cast<void*>(populateFb)) && writeSlot(mapper,reinterpret_cast<void*>(mapperSelect)))return true;
    writeSlot(slot,slot.saved);writeSlot(mapper,mapper.saved);return false;
}
rk_mode_cache_ops modeOps{};
int hookedModeAttribute(void *d,uint32_t id,int attr,int *value) {
    return rk_mode_attribute(&modeOps,d,id,attr,value);
}
int hookedModeActive(void *d,uint32_t id) {
    // Record only configuration IDs and membership, never media or buffers.
    void *c=rk_mode_ptr(d,0x1f8);
    bool reported=rk_mode_find(&modeOps,d,id)!=nullptr,raw=false;
    uintptr_t b=(uintptr_t)rk_mode_ptr(c,0x158),e=(uintptr_t)rk_mode_ptr(c,0x160);
    if(e>=b && (e-b)%0x60==0 && (e-b)/0x60<=4096)
        for(uintptr_t p=b;p<e;p+=0x60)if(modeOps.id((void*)p)==id){raw=true;break;}
    int result=rk_mode_active(&modeOps,d,id);
    __android_log_print(ANDROID_LOG_INFO,"RKModeCache","config=%u reported=%d raw=%d result=%d",id,reported,raw,result);
    return result;
}
bool writeSlot(const Slot &slot,void *value) {
    long size=sysconf(_SC_PAGESIZE);uintptr_t page=(uintptr_t)slot.pointer & ~(uintptr_t(size)-1);
    if(mprotect((void*)page,size,slot.protection|PROT_WRITE))return false;
    __atomic_store_n(slot.pointer,value,__ATOMIC_RELEASE);
    return !mprotect((void*)page,size,slot.protection);
}
int protections(void *p) {
    FILE *file=fopen("/proc/self/maps","r");if(!file)return -1;
    char line[512],perms[5];unsigned long a,b;int result=-1;
    while(fgets(line,sizeof(line),file))if(sscanf(line,"%lx-%lx %4s",&a,&b,perms)==3 && (uintptr_t)p>=a && (uintptr_t)p<b){
        result=(perms[0]=='r'?PROT_READ:0)|(perms[1]=='w'?PROT_WRITE:0)|(perms[2]=='x'?PROT_EXEC:0);break;
    }
    fclose(file);return result;
}
bool prepareModeCache(void *module,Slot *attribute,Slot *active,rk_mode_cache_ops *ops) {
    Dl_info info{};if(!dladdr(module,&info))return false;
    uintptr_t base=(uintptr_t)info.dli_fbase;auto elf=(Elf64_Ehdr*)base;
    if(memcmp(elf->e_ident,ELFMAG,SELFMAG) || elf->e_machine!=EM_AARCH64)return false;
    void *lib=dlopen(info.dli_fname,RTLD_NOW|RTLD_LOCAL|RTLD_NOLOAD);if(!lib)return false;
    const char *attributeName="_ZN7android9DrmHwcTwo10HwcDisplay19GetDisplayAttributeEjiPi";
    const char *activeName="_ZN7android9DrmHwcTwo10HwcDisplay15SetActiveConfigEj";
    ops->original_attribute=(decltype(ops->original_attribute))dlsym(lib,attributeName);
    ops->original_active=(decltype(ops->original_active))dlsym(lib,activeName);
    bool ok=(uintptr_t)ops->original_attribute==base+0x3a4c4 && (uintptr_t)ops->original_active==base+0x39904;
#define MODE_SYM(field,name) ops->field=(decltype(ops->field))dlsym(lib,name);ok=ok&&ops->field
    MODE_SYM(id,"_ZNK7android7DrmMode2idEv");
    MODE_SYM(width,"_ZNK7android7DrmMode9h_displayEv");
    MODE_SYM(height,"_ZNK7android7DrmMode9v_displayEv");
    MODE_SYM(refresh,"_ZNK7android7DrmMode9v_refreshEv");
    MODE_SYM(mm_width,"_ZNK7android12DrmConnector8mm_widthEv");
    MODE_SYM(mm_height,"_ZNK7android12DrmConnector9mm_heightEv");
    MODE_SYM(best,"_ZN7android12DrmConnector13set_best_modeERKNS_7DrmModeE");
    MODE_SYM(current,"_ZN7android12DrmConnector16set_current_modeERKNS_7DrmModeE");
    MODE_SYM(update,"_ZN7android9DrmDevice17UpdateDisplayModeEi");
    MODE_SYM(display_info,"_ZN7android12DrmConnector18SetDisplayModeInfoEi");
#undef MODE_SYM
    dlclose(lib);if(!ok)return false;
    auto ph=(Elf64_Phdr*)(base+elf->e_phoff);Elf64_Dyn *dyn=nullptr;
    for(unsigned i=0;i<elf->e_phnum;++i)if(ph[i].p_type==PT_DYNAMIC)dyn=(Elf64_Dyn*)(base+ph[i].p_vaddr);
    if(!dyn)return false;
    auto address=[&](uintptr_t v){return v<base?base+v:v;};
    Elf64_Sym *sym=nullptr;const char *str=nullptr;Elf64_Rela *rela=nullptr;size_t bytes=0;
    for(auto d=dyn;d->d_tag!=DT_NULL;++d){
        if(d->d_tag==DT_SYMTAB)sym=(Elf64_Sym*)address(d->d_un.d_ptr);
        if(d->d_tag==DT_STRTAB)str=(const char*)address(d->d_un.d_ptr);
        if(d->d_tag==DT_JMPREL)rela=(Elf64_Rela*)address(d->d_un.d_ptr);
        if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
    }
    if(!sym || !str || !rela || bytes>1024*1024)return false;
    unsigned count=0;
    for(size_t i=0;i<bytes/sizeof(*rela);++i){
        const char *name=str+sym[ELF64_R_SYM(rela[i].r_info)].st_name;Slot *slot=nullptr;
        if(!strcmp(name,attributeName))slot=attribute;
        if(!strcmp(name,activeName))slot=active;
        if(slot && ELF64_R_TYPE(rela[i].r_info)==R_AARCH64_JUMP_SLOT){
            if(slot->pointer)return false;
            slot->pointer=(void**)(base+rela[i].r_offset);slot->saved=*slot->pointer;slot->protection=protections(slot->pointer);
            if(slot->protection<0)return false;++count;
        }
    }
    return count==2 && attribute->saved==(void*)ops->original_attribute && active->saved==(void*)ops->original_active;
}
bool installModeCache(void *module) {
    Slot attribute{},active{};rk_mode_cache_ops ops{};
    if(!prepareModeCache(module,&attribute,&active,&ops))return false;
    modeOps=ops;
    if(!writeSlot(attribute,(void*)hookedModeAttribute) || !writeSlot(active,(void*)hookedModeActive)){
        writeSlot(attribute,attribute.saved);writeSlot(active,active.saved);return false;
    }
    return true;
}
bool slots(void *module,Slot *add,Slot *commit,Slot *release,Slot *hdrSwitch,Slot *propertyGet) {
    Dl_info info{};if(!dladdr(module,&info))return false;
    uintptr_t base=(uintptr_t)info.dli_fbase;auto elf=(Elf64_Ehdr*)base;
    if(memcmp(elf->e_ident,ELFMAG,SELFMAG) || elf->e_machine!=EM_AARCH64)return false;
    auto ph=(Elf64_Phdr*)(base+elf->e_phoff);Elf64_Dyn *dyn=nullptr;
    for(unsigned i=0;i<elf->e_phnum;++i)if(ph[i].p_type==PT_DYNAMIC)dyn=(Elf64_Dyn*)(base+ph[i].p_vaddr);
    if(!dyn)return false;
    auto address=[&](uintptr_t v){return v<base?base+v:v;};
    Elf64_Sym *sym=nullptr;const char *str=nullptr;Elf64_Rela *rela=nullptr;size_t bytes=0;
    for(auto d=dyn;d->d_tag!=DT_NULL;++d){
        if(d->d_tag==DT_SYMTAB)sym=(Elf64_Sym*)address(d->d_un.d_ptr);
        if(d->d_tag==DT_STRTAB)str=(const char*)address(d->d_un.d_ptr);
        if(d->d_tag==DT_JMPREL)rela=(Elf64_Rela*)address(d->d_un.d_ptr);
        if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
    }
    if(!sym || !str || !rela || bytes>1024*1024)return false;
    unsigned count=0;
    for(size_t i=0;i<bytes/sizeof(*rela);++i){
        const char *name=str+sym[ELF64_R_SYM(rela[i].r_info)].st_name;Slot *slot=nullptr;
        if(!strcmp(name,"drmModeAtomicAddProperty"))slot=add;
        if(!strcmp(name,"drmModeAtomicCommit"))slot=commit;
        if(!strcmp(name,"drmModeAtomicFree"))slot=release;
        if(!strcmp(name,"_ZN7android9DrmHwcTwo10HwcDisplay13SwitchHdrModeEv"))slot=hdrSwitch;
        if(!strcmp(name,"property_get"))slot=propertyGet;
        if(slot && ELF64_R_TYPE(rela[i].r_info)==R_AARCH64_JUMP_SLOT){
            slot->pointer=(void**)(base+rela[i].r_offset);slot->saved=*slot->pointer;slot->protection=protections(slot->pointer);
            if(slot->protection<0)return false;++count;
        }
    }
    return count==5;
}
bool install(void *original) {
    Slot add{},commit{},release{},hdrSwitch{},propertyGet{};
    if(!slots(original,&add,&commit,&release,&hdrSwitch,&propertyGet))return false;
    if(add.saved==(void*)hookedAdd && commit.saved==(void*)hookedCommit && release.saved==(void*)hookedFree &&
            hdrSwitch.saved==(void*)hookedSwitchHdr && propertyGet.saved==(void*)hookedPropertyGet)return true;
    void *drm=dlopen("libdrm.so",RTLD_NOW|RTLD_LOCAL);if(!drm)return false;
    realAdd=(Add)add.saved;realCommit=(Commit)commit.saved;
    realCreate=(Create)dlsym(drm,"drmModeCreatePropertyBlob");
    duplicate=(decltype(duplicate))dlsym(drm,"drmModeAtomicDuplicate");
    freeReq=(decltype(freeReq))release.saved;
    destroyBlob=(decltype(destroyBlob))dlsym(drm,"drmModeDestroyPropertyBlob");
    realSwitchHdr=(decltype(realSwitchHdr))hdrSwitch.saved;
    realPropertyGet=(decltype(realPropertyGet))propertyGet.saved;
    if(!realCreate || !duplicate || !freeReq || !destroyBlob)return false;
    Slot entries[]={add,commit,release,hdrSwitch,propertyGet};
    void *hooks[]={(void*)hookedAdd,(void*)hookedCommit,(void*)hookedFree,(void*)hookedSwitchHdr,(void*)hookedPropertyGet};
    for(size_t i=0;i<5;++i)if(!writeSlot(entries[i],hooks[i])){
        for(size_t j=i+1;j>0;--j)writeSlot(entries[j-1],entries[j-1].saved);
        return false;
    }
    return true;
}
hw_module_t *loadOriginal(const char *path) {
    void *lib=dlopen(path,RTLD_NOW|RTLD_LOCAL);if(!lib){__android_log_print(ANDROID_LOG_ERROR,"RKHdrHwc","load original: %s",dlerror());return nullptr;}
    return (hw_module_t*)dlsym(lib,HAL_MODULE_INFO_SYM_AS_STR);
}
int openModule(const hw_module_t*,const char *id,hw_device_t **device) {
#ifdef RKQMS_HWC_CANDIDATE
    if(qmsBootRequested() && !qmsStart())return -ENODEV;
#endif
    auto module=loadOriginal("/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so");if(!module || !module->methods)return -ENODEV;
    int result=module->methods->open(module,id,device);if(result || !*device)return result;
    auto hwc=(hwc2_device_t*)*device;
    if(install(module)){originalGet=hwc->getFunction;hwc->getFunction=getFunction;
        clientHdr10=property_get_bool("debug.rkhdr.client_hdr10",false);
        if(clientHdr10){
            bool ok=installTargetPlan(module);property_set("vendor.rkhdr.target_plan",ok?"active":"failed");
            if(!ok)clientHdr10=false;
        } else property_set("vendor.rkhdr.target_plan","off");
        if(property_get_bool("debug.rkhdr.mode_cache",false)) {
            bool ok=installModeCache(module);
            property_set("vendor.rkhdr.mode_cache",ok?"active":"failed");
            __android_log_print(ok?ANDROID_LOG_INFO:ANDROID_LOG_ERROR,"RKModeCache","mode cache adapter %s",ok?"active":"failed");
        } else property_set("vendor.rkhdr.mode_cache","off");
        int propertyResult=property_set("vendor.rkhdr.adapter","active");
        __android_log_print(ANDROID_LOG_INFO,"RKHdrHwc","metadata adapter active; original compositor retained, property=%d",propertyResult);}
    else __android_log_print(ANDROID_LOG_ERROR,"RKHdrHwc","metadata adapter unavailable; original compositor retained");
    return result;
}
hw_module_methods_t methods={.open=openModule};
}
extern "C" {
__attribute__((visibility("default"))) int rk_hdr_target_plan_check(const char* path){
    auto module=loadOriginal(path);Slot slot{},mapper{};return module && prepareTargetPlan(module,&slot) && prepareMapperSelect(&mapper)?0:1;
}
__attribute__((visibility("default"))) int rk_hdr_mode_cache_check(const char *path) {
    auto module=loadOriginal(path);Slot a{},b{};rk_mode_cache_ops ops{};
    if(!module || !prepareModeCache(module,&a,&b,&ops))return 1;
    if(!writeSlot(a,(void*)hookedModeAttribute) || !writeSlot(b,(void*)hookedModeActive)){
        writeSlot(a,a.saved);writeSlot(b,b.saved);return 2;
    }
    return writeSlot(a,a.saved)&&writeSlot(b,b.saved)?0:3;
}
__attribute__((visibility("default"))) hw_module_t HAL_MODULE_INFO_SYM={
    .tag=HARDWARE_MODULE_TAG,.module_api_version=HWC_MODULE_API_VERSION_0_1,
    .hal_api_version=HARDWARE_HAL_API_VERSION,.id=HWC_HARDWARE_MODULE_ID,
    .name="RK3588 static HDR metadata adapter",.author="sw1313",.methods=&methods,
};
__attribute__((visibility("default"))) int rk_hdr_hwc_check(const char *path) {
    auto module=loadOriginal(path);Slot add{},commit{},release{},hdrSwitch{},propertyGet{};
    if(!module || !slots(module,&add,&commit,&release,&hdrSwitch,&propertyGet))return 1;
    // Exercise the actual write/restore operation only in this diagnostic
    // process, without opening a compositor device or touching live services.
    if(!writeSlot(add,add.saved) || !writeSlot(commit,commit.saved) || !writeSlot(release,release.saved) ||
            !writeSlot(hdrSwitch,hdrSwitch.saved) || !writeSlot(propertyGet,propertyGet.saved))return 2;
    auto savedGet=realPropertyGet;bool savedScope=clientOutputSdr;
    realPropertyGet=(decltype(realPropertyGet))propertyGet.saved;
    char reference[PROPERTY_VALUE_MAX]{},actual[PROPERTY_VALUE_MAX]{};
    int referenceLen=realPropertyGet("vendor.hwc.hdr_force_disable",reference,"0");
    clientOutputSdr=true;
    bool ok=hookedPropertyGet("vendor.hwc.hdr_force_disable",actual,"0")==1 && !strcmp(actual,"1");
    ok &= hookedPropertyGet("debug.rkhdr.check.nonexistent",actual,"pass-through")==12 && !strcmp(actual,"pass-through");
    clientOutputSdr=false;
    ok &= hookedPropertyGet("vendor.hwc.hdr_force_disable",actual,"0")==referenceLen && !strcmp(actual,reference);
    clientOutputSdr=savedScope;realPropertyGet=savedGet;
    if(!ok)return 3;
    return 0;
}
}
