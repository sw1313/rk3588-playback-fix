// SPDX-License-Identifier: MIT
#pragma once
#include <C2Config.h>
#include <C2AllocatorGralloc.h>
#include "mpp/mpp_frame.h"
#include "Hdr10PlusPayload.h"
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <gralloctypes/Gralloc4.h>
#include <ui/GraphicBufferMapper.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <android/log.h>
#include <cstring>
#include <optional>
#include <vector>

namespace android { namespace rk_hdr {

inline std::shared_ptr<C2StreamHdr10PlusInfo::output> plus(MppFrame frame) {
    struct Dynamic {uint32_t format, size;uint8_t data[];};
    using Getter=Dynamic *(*)(MppFrame);
    static Getter getter=reinterpret_cast<Getter>(dlsym(RTLD_DEFAULT,"mpp_frame_get_hdr_dynamic_meta"));
    auto info=C2StreamHdr10PlusInfo::output::AllocShared(0,0u);
    if(!getter)return info;
    const Dynamic *meta=getter(frame);
    if(!meta || meta->format!=8 || !validPlus(meta->data,meta->size))return info;
    static const uint8_t prefix[]={0xb5,0x00,0x3c,0x00,0x01,0x04};
    info=C2StreamHdr10PlusInfo::output::AllocShared(sizeof(prefix)+meta->size,0u);
    std::memcpy(info->m.value,prefix,sizeof(prefix));
    std::memcpy(info->m.value+sizeof(prefix),meta->data,meta->size);
    return info;
}

// Only metadata is touched. No buffer allocation, pixel lock, conversion or
// additional image copy is performed. Import and release a duplicate handle.
inline bool store(const std::shared_ptr<C2GraphicBlock> &block,
                  const std::shared_ptr<C2StreamHdrStaticInfo::output> &hdr,
                  const std::shared_ptr<C2StreamHdr10PlusInfo::output> &dynamic) {
    (void)dynamic;
    // Dynamic metadata travels in the C2Buffer/BufferQueue standard info,
    // including explicit empty frames. The stock mapper rejects clearing its
    // variable-size HDR10+ field, so never populate that separate buffer field.
    if(!property_get_bool("debug.rkcodec2.hdr_static_buffer",false))return true;
    using android::hardware::graphics::mapper::V4_0::IMapper;
    using android::hardware::graphics::mapper::V4_0::Error;
    using android::hardware::hidl_vec;
    static sp<IMapper> mapper=IMapper::getService();
    if(!mapper || !block || !block->handle()) {
        __android_log_print(ANDROID_LOG_ERROR,"RKHdrBridge","missing mapper=%d block=%d",!!mapper,!!block);return false;
    }
    auto c2=block->handle();uint32_t width,height,format,stride,generation,slot;
    uint64_t usage,id;
    _UnwrapNativeCodec2GrallocMetadata(c2,&width,&height,&format,&usage,&stride,&generation,&id,&slot);
    native_handle_t *raw=UnwrapNativeCodec2GrallocHandle(c2);
    if(!raw)return false;
    buffer_handle_t imported=nullptr;
    auto &gm=GraphicBufferMapper::get();
    status_t status=gm.importBuffer(raw,width,height,1,format,usage,stride,&imported);
    native_handle_delete(raw);
    if(status!=OK || !imported) {
        __android_log_print(ANDROID_LOG_ERROR,"RKHdrBridge","import=%d %ux%u fmt=%u stride=%u",status,width,height,format,stride);return false;
    }
    bool ok=true;
    auto set=[&](const IMapper::MetadataType &type,const hidl_vec<uint8_t> &encoded) {
        auto r=mapper->set(const_cast<native_handle_t*>(imported),type,encoded);
        if(!r.isOk() || r.withDefault(Error::NO_RESOURCES)!=Error::NONE) {
            __android_log_print(ANDROID_LOG_ERROR,"RKHdrBridge","set %s/%lld len=%zu error=%d",type.name.c_str(),(long long)type.value,encoded.size(),int(r.withDefault(Error::NO_RESOURCES)));ok=false;
        }
    };
    if(property_get_bool("debug.rkcodec2.hdr_static_buffer",false)) {
        // This firmware rejects zero-length (nullopt) encoded metadata. Encode
        // explicit zero values to clear a reused buffer without stale HDR data.
        std::optional<ui::Smpte2086> mastering=ui::Smpte2086{};
        std::optional<ui::Cta861_3> light=ui::Cta861_3{};
        if(hdr && hdr->mastering.maxLuminance>0) {
            ui::Smpte2086 v;
            v.primaryRed={hdr->mastering.red.x,hdr->mastering.red.y};
            v.primaryGreen={hdr->mastering.green.x,hdr->mastering.green.y};
            v.primaryBlue={hdr->mastering.blue.x,hdr->mastering.blue.y};
            v.whitePoint={hdr->mastering.white.x,hdr->mastering.white.y};
            v.maxLuminance=hdr->mastering.maxLuminance;
            v.minLuminance=hdr->mastering.minLuminance;mastering=v;
        }
        if(hdr && (hdr->maxCll>0 || hdr->maxFall>0)) {
            ui::Cta861_3 v;v.maxContentLightLevel=hdr->maxCll;v.maxFrameAverageLightLevel=hdr->maxFall;light=v;
        }
        hidl_vec<uint8_t> bytes;
        if(gralloc4::encodeSmpte2086(mastering,&bytes)==OK)set(gralloc4::MetadataType_Smpte2086,bytes);else ok=false;
        if(gralloc4::encodeCta861_3(light,&bytes)==OK)set(gralloc4::MetadataType_Cta861_3,bytes);else ok=false;
    }
    gm.freeBuffer(imported);return ok;
}
} }
