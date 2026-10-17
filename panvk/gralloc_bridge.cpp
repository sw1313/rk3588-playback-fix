// SPDX-License-Identifier: MIT
// Read-only Android 12 libui adapter. Fail closed if the pinned ABI is absent.
#include "gralloc_bridge.h"
#include <android/log.h>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <vector>
#include <cstddef>

namespace {
// Field layout from Android 12 graphics/common AIDL PlaneLayout,
// PlaneLayoutComponent and ExtendableType. No binder methods are called.
struct ExtendableType { std::string name; int64_t value; };
struct Component { ExtendableType type; int64_t offset, size; };
struct Plane {
    std::vector<Component> components;
    int64_t offset, sample_bits, pitch, width, height, size, hsub, vsub;
};
static_assert(sizeof(ExtendableType) == 32 && sizeof(Component) == 48);
static_assert(sizeof(Plane) == 88 && offsetof(Plane, pitch) == 40);
using GetInstance = void* (*)();
using Scalar32 = int (*)(void*, const native_handle_t*, uint32_t*);
using Scalar64 = int (*)(void*, const native_handle_t*, uint64_t*);
using Planes = int (*)(void*, const native_handle_t*, std::vector<Plane>*);
std::once_flag once;
void* mapper;
Scalar32 get_fourcc;
Scalar64 get_modifier, get_allocation;
Planes get_planes;
void init() {
    // libnativewindow is a public NDK library. Its existing dependency scope
    // includes the ROM's own libui and mapper, avoiding a second vendor HAL.
    void* nw = dlopen("libnativewindow.so", RTLD_NOW | RTLD_LOCAL);
    if (!nw) return;
    auto get = reinterpret_cast<GetInstance>(dlsym(nw,
        "_ZN7android9SingletonINS_19GraphicBufferMapperEE11getInstanceEv"));
    get_fourcc = reinterpret_cast<Scalar32>(dlsym(nw,
        "_ZN7android19GraphicBufferMapper20getPixelFormatFourCCEPK13native_handlePj"));
    get_modifier = reinterpret_cast<Scalar64>(dlsym(nw,
        "_ZN7android19GraphicBufferMapper22getPixelFormatModifierEPK13native_handlePm"));
    get_allocation = reinterpret_cast<Scalar64>(dlsym(nw,
        "_ZN7android19GraphicBufferMapper17getAllocationSizeEPK13native_handlePm"));
    get_planes = reinterpret_cast<Planes>(dlsym(nw,
        "_ZN7android19GraphicBufferMapper15getPlaneLayoutsEPK13native_handlePNSt3__16vectorIN4aidl7android8hardware8graphics6common11PlaneLayoutENS4_9allocatorISB_EEEE"));
    if (get && get_fourcc && get_modifier && get_allocation && get_planes)
        mapper = get();
    __android_log_print(ANDROID_LOG_INFO, "EdenRC3", "ROM mapper ABI %s",
                        mapper ? "available" : "unavailable");
}
}
extern "C" int eden_query_gralloc(const native_handle_t* handle,
                                   eden_gralloc_layout* out) {
    std::call_once(once, init);
    if (!mapper || !handle || handle->numFds < 1) return -1;
    std::vector<Plane> planes;
    eden_gralloc_layout layout = {};
    if (get_fourcc(mapper, handle, &layout.fourcc) ||
        get_modifier(mapper, handle, &layout.modifier) ||
        get_allocation(mapper, handle, &layout.allocation) ||
        get_planes(mapper, handle, &planes)) return -2;
    // The window bridge currently supports one RGB plane only. Do not guess
    // multi-fd/video/protected layouts or assume a missing modifier is linear.
    if (planes.size() != 1 || planes[0].offset < 0 || planes[0].pitch <= 0 ||
        planes[0].size <= 0 || !layout.allocation || layout.modifier == ~uint64_t(0) ||
        uint64_t(planes[0].offset) > layout.allocation ||
        uint64_t(planes[0].size) > layout.allocation - uint64_t(planes[0].offset))
        return -3;
    layout.offset = planes[0].offset;
    layout.pitch = planes[0].pitch;
    layout.fd = handle->data[0];
    *out = layout;
    __android_log_print(ANDROID_LOG_INFO, "EdenRC3",
        "ROM layout fourcc=%08x modifier=%016llx pitch=%llu offset=%llu size=%llu",
        layout.fourcc, (unsigned long long)layout.modifier,
        (unsigned long long)layout.pitch, (unsigned long long)layout.offset,
        (unsigned long long)layout.allocation);
    return 0;
}
