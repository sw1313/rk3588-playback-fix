// SPDX-License-Identifier: MIT
// Exercises real dma-buf backed RGA jobs without changing system libraries.
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <android/hardware_buffer.h>
#include <vndk/hardware_buffer.h>
#include "RgaApi.h"

struct heap_allocation { uint64_t len; uint32_t fd; uint32_t flags; uint64_t heap_flags; };
#define HEAP_ALLOC _IOWR('H', 0, struct heap_allocation)
struct dma_sync { uint64_t flags; };
#define DMA_SYNC _IOW('b', 0, struct dma_sync)

struct Buffer {
    int fd = -1; size_t size = 0; uint8_t *bytes = nullptr;
    bool allocate(size_t n) {
        int heap = open("/dev/dma_heap/system-uncached-dma32", O_RDONLY | O_CLOEXEC);
        if (heap < 0) return false;
        heap_allocation a = { (n + 4095) & ~uint64_t(4095), 0, O_RDWR | O_CLOEXEC, 0 };
        int rc = ioctl(heap, HEAP_ALLOC, &a); close(heap);
        if (rc) return false;
        fd = a.fd; size = a.len;
        void *p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) { close(fd); fd = -1; return false; }
        bytes = static_cast<uint8_t *>(p); return true;
    }
    bool sync(bool end) {
        dma_sync a = { uint64_t(3 | (end ? 4 : 0)) };
        return ioctl(fd, DMA_SYNC, &a) == 0;
    }
    ~Buffer() { if (bytes) munmap(bytes, size); if (fd >= 0) close(fd); }
};

static rga_info_t info(Buffer &b, int w, int h, int stride, int format) {
    rga_info_t i = {}; i.fd = b.fd; i.mmuFlag = 1;
    i.in_fence_fd = -1; i.out_fence_fd = -1; i.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&i.rect, 0, 0, w, h, stride, h, format);
    return i;
}

using Blit = int (*)(rga_info_t *, rga_info_t *, rga_info_t *);

static int graphic(Blit blit) {
    AHardwareBuffer_Desc desc = {};
    desc.width=640; desc.height=360; desc.layers=1;
    desc.format=AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage=AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    AHardwareBuffer *src=nullptr, *dst=nullptr;
    int rc=AHardwareBuffer_allocate(&desc,&src);
    if(rc)return 40;
    desc.width=320; desc.height=180;
    rc=AHardwareBuffer_allocate(&desc,&dst);
    if(rc) { AHardwareBuffer_release(src); return 41; }
    AHardwareBuffer_Desc sd,dd;
    AHardwareBuffer_describe(src,&sd); AHardwareBuffer_describe(dst,&dd);
    void *map=nullptr;
    rc=AHardwareBuffer_lock(src,AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,-1,nullptr,&map);
    if(rc) { AHardwareBuffer_release(src);AHardwareBuffer_release(dst);return 42; }
    for(unsigned y=0;y<sd.height;++y)for(unsigned x=0;x<sd.width;++x) {
        uint8_t *p=static_cast<uint8_t *>(map)+(size_t(y)*sd.stride+x)*4;
        p[0]=x<sd.width/2 ? 220 : 20; p[1]=y<sd.height/2 ? 200 : 30;
        p[2]=60;p[3]=255;
    }
    rc=AHardwareBuffer_unlock(src,nullptr);
    rga_info_t s={},d={}; s.fd=-1;d.fd=-1;s.mmuFlag=1;d.mmuFlag=1;
    s.hnd=AHardwareBuffer_getNativeHandle(src);d.hnd=AHardwareBuffer_getNativeHandle(dst);
    s.in_fence_fd=d.in_fence_fd=-1;s.out_fence_fd=d.out_fence_fd=-1;
    s.sync_mode=d.sync_mode=RGA_BLIT_SYNC;
    rga_set_rect(&s.rect,0,0,sd.width,sd.height,sd.stride,sd.height,RK_FORMAT_RGBA_8888);
    rga_set_rect(&d.rect,0,0,dd.width,dd.height,dd.stride,dd.height,RK_FORMAT_RGBA_8888);
    if(!rc)rc=blit(&s,&d,nullptr);
    if(rc) { printf("GraphicBuffer blit failed %d\n",rc); AHardwareBuffer_release(src);AHardwareBuffer_release(dst);return 43; }
    rc=AHardwareBuffer_lock(dst,AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,-1,nullptr,&map);
    if(rc) { AHardwareBuffer_release(src);AHardwareBuffer_release(dst);return 44; }
    int bad=0;
    for(unsigned y=16;y<dd.height;y+=29)for(unsigned x=16;x<dd.width;x+=31) {
        const uint8_t *p=static_cast<uint8_t *>(map)+(size_t(y)*dd.stride+x)*4;
        bad+=(p[0]!=(x<dd.width/2 ? 220 : 20) || p[1]!=(y<dd.height/2 ? 200 : 30) || p[2]!=60 || p[3]!=255);
    }
    rc=AHardwareBuffer_unlock(dst,nullptr);AHardwareBuffer_release(src);AHardwareBuffer_release(dst);
    printf("GraphicBuffer 640x360->320x180: mismatches=%d\n",bad);
    return (rc || bad) ? 45 : 0;
}

static int pixels(Blit blit, int w, int h, int padding) {
    int stride = w + padding;
    Buffer source, target, rgb;
    if (!source.allocate(size_t(stride) * h * 3 / 2) ||
        !target.allocate(size_t(stride) * h * 3 / 2) ||
        !rgb.allocate(size_t(w) * h * 4)) return 20;
    if (!source.sync(false) || !target.sync(false) || !rgb.sync(false)) return 21;
    memset(source.bytes, 0xa5, source.size); memset(target.bytes, 0x77, target.size);
    memset(rgb.bytes, 0x55, rgb.size);
    for (int y=0; y<h; ++y) for (int x=0; x<w; ++x)
        source.bytes[size_t(y)*stride+x] = 16 + ((x/32 + y/32) % 8) * 25;
    for (int y=0; y<h/2; ++y) for (int x=0; x<w; x+=2) {
        source.bytes[size_t(stride)*h+size_t(y)*stride+x] = 128;
        source.bytes[size_t(stride)*h+size_t(y)*stride+x+1] = 128;
    }
    if (!source.sync(true) || !target.sync(true) || !rgb.sync(true)) return 22;
    auto s=info(source,w,h,stride,RK_FORMAT_YCbCr_420_SP);
    auto d=info(target,w,h,stride,RK_FORMAT_YCbCr_420_SP);
    int rc=blit(&s,&d,nullptr); if (rc) { printf("NV12 blit failed %d\n",rc); return 23; }
    if (!target.sync(false) || !source.sync(false)) return 24;
    size_t bad=0;
    for (int y=0;y<h*3/2;++y) for(int x=0;x<w;++x)
        bad += target.bytes[size_t(y)*stride+x] != source.bytes[size_t(y)*stride+x];
    if (!target.sync(true) || !source.sync(true)) return 25;
    if (bad) { printf("NV12 mismatch %zu\n",bad); return 26; }
    auto rgba=info(rgb,w,h,w,RK_FORMAT_RGBA_8888);
    rc=blit(&s,&rgba,nullptr); if(rc) { printf("NV12->RGBA failed %d\n",rc); return 27; }
    if (!rgb.sync(false)) return 28;
    int max_error=0;
    for(int y=0;y<h;y+=17) for(int x=0;x<w;x+=19) {
        int luma=16+((x/32+y/32)%8)*25;
        int expected=((luma-16)*298+128)>>8;
        const uint8_t *p=rgb.bytes+(size_t(y)*w+x)*4;
        for(int c=0;c<3;++c) { int e=abs(int(p[c])-expected); if(e>max_error)max_error=e; }
        if(p[3]!=255) { printf("RGBA alpha mismatch\n"); return 29; }
    }
    if (!rgb.sync(true)) return 30;
    printf("PASS NV12 copy + RGBA %dx%d stride=%d error=%d\n",w,h,stride,max_error);
    return max_error>4 ? 31 : 0;
}

int main(int argc, char **argv) {
    setbuf(stdout,nullptr);
    if(argc<3) { fprintf(stderr,"usage: rga_probe LIB CONTRACT [pixels]\n"); return 2; }
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL);
    if(!lib) { printf("LOAD_FAIL %s\n",dlerror()); return 3; }
    FILE *contract=fopen(argv[2],"r"); if(!contract)return 4;
    char symbol[1024]; int count=0;
    while(fgets(symbol,sizeof(symbol),contract)) {
        symbol[strcspn(symbol,"\r\n")]=0;
        if(!symbol[0] || symbol[0]=='#')continue;
        if(!dlsym(lib,symbol)) { printf("MISSING %s\n",symbol); fclose(contract); return 5; }
        ++count;
    }
    fclose(contract); printf("PASS ABI %d symbols\n",count);
    const char *(*query)(int)=reinterpret_cast<const char *(*)(int)>(dlsym(lib,"querystring"));
    const char *version=query ? query(1) : nullptr;
    if(version)printf("VERSION %s\n",version);
    if(argc>=5 && (!version || !strstr(version,argv[4])))return 7;
    if(argc>=6 && !strcmp(argv[5],"component")) {
        const char *path=sizeof(void*)==8 ? "/vendor/lib64/libcodec2_rk_component.so" : "/vendor/lib/libcodec2_rk_component.so";
        void *component=dlopen(path,RTLD_NOW|RTLD_LOCAL);
        if(!component) { printf("CODEC2_LOAD_FAIL %s\n",dlerror());return 8; }
        printf("PASS Codec2 loading\n");dlclose(component);
    }
    if(argc<4 || strcmp(argv[3],"pixels"))return 0;
    Blit blit=reinterpret_cast<Blit>(dlsym(lib,"c_RkRgaBlit")); if(!blit)return 6;
    for(int size=0; size<4; ++size) {
        const int widths[]={320,1280,1920,3840}, heights[]={240,720,1080,2160};
        int rc=pixels(blit,widths[size],heights[size],size==2 ? 16 : 0);
        if(rc)return rc;
    }
    return graphic(blit);
}
