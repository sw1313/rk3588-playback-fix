// SPDX-License-Identifier: MIT
// Exact HEVC native-composer NV15/P010 readback. ImageReader acquisition waits
// for the actual acquire fence; DMA-BUF read synchronization is always paired.
// Diagnostic only: never enables compression, changes stream pixels or displays
// this ImageReader. Mapper-provided plane offsets/strides are authoritative.
#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <android/hardware_buffer.h>
#include <ui/GraphicBufferMapper.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <map>

static double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static int expectedFormat = 23;
static bool dump_image(AImage *image, FILE *output, const AImageCropRect &crop, int bits) {
    AHardwareBuffer *buffer = nullptr;
    if (bits != 10 || AImage_getHardwareBuffer(image, &buffer) || !buffer) return false;
    AHardwareBuffer_Desc desc{}; AHardwareBuffer_describe(buffer, &desc);
    static void *nw = dlopen("libnativewindow.so", RTLD_NOW);
    static auto handleFn = nw ? reinterpret_cast<const native_handle_t *(*)(const AHardwareBuffer *)>(dlsym(nw,"AHardwareBuffer_getNativeHandle")) : nullptr;
    auto handle = handleFn ? handleFn(buffer) : nullptr;
    if (!handle || handle->numFds < 1 || desc.format != unsigned(expectedFormat) ||
        (desc.format != 23 && desc.format != 54) || crop.left < 0 || crop.top < 0 ||
        crop.right > int(desc.width) || crop.bottom > int(desc.height) ||
        crop.right <= crop.left || crop.bottom <= crop.top ||
        ((crop.left|crop.top|crop.right|crop.bottom)&1)) return false;
    std::vector<android::ui::PlaneLayout> layouts;
    auto status = android::GraphicBufferMapper::get().getPlaneLayouts(handle, &layouts);
    printf("IMAGE format=0x%x usage=0x%llx dimensions=%ux%u mapper=%d planes=%zu\n", desc.format,(unsigned long long)desc.usage,desc.width,desc.height,status,layouts.size());
    if (status || layouts.size()<2) return false;
    for (auto &p:layouts) printf("PLANE offset=%lld stride=%lld size=%lld increment=%lld\n",(long long)p.offsetInBytes,(long long)p.strideInBytes,(long long)p.totalSizeInBytes,(long long)p.sampleIncrementInBits);
    int fd=handle->data[0]; struct stat st{};
    if(fstat(fd,&st)||st.st_size<=0)return false;
    dma_buf_sync sync{DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ};
    if(ioctl(fd,DMA_BUF_IOCTL_SYNC,&sync))return false;
    void *mapping=mmap(nullptr,st.st_size,PROT_READ,MAP_SHARED,fd,0);
    bool ok=mapping!=MAP_FAILED;
    if(ok){
        auto bytes=static_cast<const uint8_t*>(mapping);
        unsigned width=crop.right-crop.left,height=crop.bottom-crop.top;
        for(unsigned plane=0;plane<3&&ok;plane++){
            const auto &layout=layouts[plane?1:0];
            unsigned rows=plane?height/2:height,cols=plane?width/2:width;
            for(unsigned y=0;y<rows&&ok;y++)for(unsigned x=0;x<cols;x++){
                unsigned sample=(plane?crop.left+2*x+plane-1:crop.left+x);
                int64_t row=layout.offsetInBytes+(int64_t(y)+(plane?crop.top/2:crop.top))*layout.strideInBytes;
                uint64_t bit=uint64_t(sample)*(desc.format==23?10:16),offset=uint64_t(row)+bit/8;
                if(row<0||layout.strideInBytes<=0||offset+1>=uint64_t(st.st_size)||bit/8+1>=uint64_t(layout.strideInBytes)){ok=false;break;}
                uint16_t word=uint16_t(bytes[offset])|(uint16_t(bytes[offset+1])<<8);
                if(desc.format==54&&(word&63)){ok=false;break;}
                uint16_t value=desc.format==23?((word>>(bit%8))&1023):(word>>6);
                if(fwrite(&value,2,1,output)!=1){ok=false;break;}
            }
        }
        munmap(mapping,st.st_size);
    }
    sync.flags=DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ;
    if(ioctl(fd,DMA_BUF_IOCTL_SYNC,&sync))ok=false;
    return ok;
}

int main(int argc, char **argv) {
    setbuf(stdout, nullptr);
    if(argc!=5&&argc!=6){puts("Usage: native_hevc_precision CLIP FRAME_LIMIT RAW_OUTPUT EXPECTED_HAL_FORMAT [CONSUMER=0:composer,1:texture,2:explicit-P010]");return 2;} expectedFormat=atoi(argv[4]);
    int consumer=argc==6?atoi(argv[5]):0;if(consumer<0||consumer>2)return 2;
    int limit = atoi(argv[2]);
    int bits = 10;
    if (bits != 8 && bits != 10) return 2;
    int fd = open(argv[1], O_RDONLY);
    struct stat st{};
    if (limit < 1 || fd < 0 || fstat(fd, &st)) return 3;
    AMediaExtractor *extractor = AMediaExtractor_new();
    if (AMediaExtractor_setDataSourceFd(extractor, fd, 0, st.st_size)) return 4;
    AMediaFormat *format = nullptr;
    for (size_t i = 0; i < AMediaExtractor_getTrackCount(extractor); ++i) {
        auto track = AMediaExtractor_getTrackFormat(extractor, i);
        const char *mime = nullptr;
        if (AMediaFormat_getString(track, "mime", &mime) && !strncmp(mime, "video/", 6)) {
            format = track; AMediaExtractor_selectTrack(extractor, i); break;
        }
        AMediaFormat_delete(track);
    }
    int width = 0, height = 0;
    if (!format || !AMediaFormat_getInt32(format, "width", &width) ||
            !AMediaFormat_getInt32(format, "height", &height)) return 5;
    // Rebuild the test format without a profile: reproduce late ten-bit SPS
    // discovery. Codec configuration, not encoded input, is varied explicitly.
    auto original=format;format=AMediaFormat_new();
    AMediaFormat_setString(format,"mime","video/hevc");
    AMediaFormat_setInt32(format,"width",width);AMediaFormat_setInt32(format,"height",height);
    for(auto key:{"csd-0","csd-1","csd-2"}){void *data=nullptr;size_t size=0;if(AMediaFormat_getBuffer(original,key,&data,&size))AMediaFormat_setBuffer(format,key,data,size);}
    AMediaFormat_delete(original);
    AMediaFormat_setInt32(format,"vendor.c2-dec-fbc-disable.value",1);
    if(consumer==2)AMediaFormat_setInt32(format,"color-format",54);
    AImageReader *reader = nullptr;
    if (AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_PRIVATE,
            consumer==1?AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE:AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY, 16, &reader)) return 6;
    ANativeWindow *window = nullptr;
    if (AImageReader_getWindow(reader, &window)) return 7;
    AMediaCodec *codec = AMediaCodec_createCodecByName("c2.rk.hevc.decoder");
    if (!codec || AMediaCodec_configure(codec, format, window, nullptr, 0) || AMediaCodec_start(codec)) return 8;
    FILE *output = fopen(argv[3], "wb");
    if (!output) return 9;
    int queued = 0, delivered = 0, images = 0;
    std::map<int64_t, AImageCropRect> crops;
    bool inputEos = false, outputEos = false, ok = true;
    double deadline = now() + 35;
    while (now() < deadline && ok && (!outputEos || images < delivered)) {
        if (!inputEos) {
            ssize_t index = AMediaCodec_dequeueInputBuffer(codec, 1000);
            if (index >= 0) {
                size_t capacity = 0;
                uint8_t *bytes = AMediaCodec_getInputBuffer(codec, index, &capacity);
                ssize_t length = queued >= limit ? -1 : AMediaExtractor_readSampleData(extractor, bytes, capacity);
                if (length < 0) {
                    inputEos = true;
                    ok = AMediaCodec_queueInputBuffer(codec, index, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) == 0;
                } else {
                    ok = AMediaCodec_queueInputBuffer(codec, index, 0, length, AMediaExtractor_getSampleTime(extractor), 0) == 0;
                    ++queued; AMediaExtractor_advance(extractor);
                }
            }
        }
        AMediaCodecBufferInfo info{};
        ssize_t index = AMediaCodec_dequeueOutputBuffer(codec, &info, 1000);
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED || (index >= 0 && delivered == 0)) {
            auto current = AMediaCodec_getOutputFormat(codec);
            if (current) { printf("MEDIA_FORMAT %s\n", AMediaFormat_toString(current)); AMediaFormat_delete(current); }
        }
        if (index >= 0) {
            bool eos = info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
            bool frame = !(info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) && (!eos || info.size > 0);
            if (frame) {
                auto current = AMediaCodec_getBufferFormat(codec, index);
                int32_t left = 0, top = 0, right = 0, bottom = 0;
                bool known = current && AMediaFormat_getRect(current, "crop", &left, &top, &right, &bottom);
                if (current) AMediaFormat_delete(current);
                if (!known || crops.count(info.presentationTimeUs)) { ok = false; break; }
                crops[info.presentationTimeUs] = AImageCropRect{left, top, right + 1, bottom + 1};
            }
            ok = AMediaCodec_releaseOutputBuffer(codec, index, frame) == 0;
            if (frame) ++delivered;
            if (eos) outputEos = true;
        }
        AImage *image = nullptr;
        while (AImageReader_acquireNextImage(reader, &image) == AMEDIA_OK) {
            int64_t timestamp = 0;
            bool matched = AImage_getTimestamp(image, &timestamp) == AMEDIA_OK && crops.count(timestamp / 1000);
            ok = matched && dump_image(image, output, crops.at(timestamp / 1000), bits) && ok;
            if (matched) crops.erase(timestamp / 1000);
            ++images; AImage_delete(image); image = nullptr;
        }
    }
    bool stopped = AMediaCodec_stop(codec) == 0;
    AMediaCodec_delete(codec); AImageReader_delete(reader);
    AMediaFormat_delete(format); AMediaExtractor_delete(extractor); close(fd);
    ok = fclose(output) == 0 && ok && outputEos && stopped && images == limit && delivered == limit;
    printf("RESULT queued=%d delivered=%d images=%d eos=%d stop=%d passed=%d\n",
           queued, delivered, images, outputEos, stopped, ok);
    return ok ? 0 : 10;
}
