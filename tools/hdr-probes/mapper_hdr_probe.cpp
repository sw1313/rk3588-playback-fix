// Inventory and isolated scratch-buffer test; never submit buffers for display.
#include <cstdio>
#include <cstring>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <ui/GraphicBuffer.h>
using android::hardware::graphics::mapper::V4_0::IMapper;
using android::hardware::graphics::mapper::V4_0::Error;
int main() {
    auto mapper=IMapper::getService();
    if (!mapper) { std::puts("ERROR mapper unavailable"); return 1; }
    bool ok=false;
    auto result=mapper->listSupportedMetadataTypes([&](Error error,const auto &types) {
        std::printf("RESULT error=%d count=%zu\n",int(error),types.size());
        ok=error==Error::NONE;
        for (const auto &t:types)
            std::printf("TYPE %s value=%lld get=%d set=%d description=%s\n",
                t.metadataType.name.c_str(),(long long)t.metadataType.value,
                int(t.isGettable),int(t.isSettable),t.description.c_str());
    });
    if (!result.isOk()) { std::printf("ERROR transport %s\n",result.description().c_str());return 1; }
    if (!ok) return 1;
    android::sp<android::GraphicBuffer> scratch = new android::GraphicBuffer(
        64,64,android::PIXEL_FORMAT_RGBA_8888,1,uint64_t(0x333),"HdrMetadataScratch");
    std::printf("SCRATCH init=%d\n",scratch->initCheck());
    if (scratch->initCheck()!=android::OK || !scratch->handle) return 1;
    auto inspect=[&](const char *name,int64_t value) {
        IMapper::MetadataType type{name,value};
        auto get=mapper->get(const_cast<native_handle_t*>(scratch->handle),type,[&](Error error,const auto &bytes) {
            std::printf("SCRATCH_GET %s value=%lld error=%d bytes=%zu\n",name,
                (long long)value,int(error),bytes.size());
        });
        if (!get.isOk()) std::printf("ERROR scratch transport %s\n",get.description().c_str());
    };
    inspect("android.hardware.graphics.common.StandardMetadataType",19);
    inspect("android.hardware.graphics.common.StandardMetadataType",20);
    inspect("android.hardware.graphics.common.StandardMetadataType",21);
    inspect("rk.graphics.RkMetadataType",1);
    android::hardware::hidl_vec<uint8_t> offset;offset.resize(sizeof(int64_t));
    std::memset(offset.data(),0,offset.size());
    auto set=mapper->set(const_cast<native_handle_t*>(scratch->handle),IMapper::MetadataType{"rk.graphics.RkMetadataType",1},offset);
    if (!set.isOk()) return 1;
    std::printf("SCRATCH_SET rk.graphics.RkMetadataType value=1 error=%d\n",int(Error(set)));
    return 0;
}
