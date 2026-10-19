// SPDX-License-Identifier: MIT
// Read back the new Codec2 GPU-consumer AFBC route through a synchronous RGA
// decompression. The image reader's synchronous acquisition owns fence waits.
#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <android/hardware_buffer.h>
#include "mpp_buffer.h"
#include "im2d.hpp"
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

static bool dump_image(AImage *image, FILE *output, const AImageCropRect &codecCrop, int bits) {
    AHardwareBuffer *buffer = nullptr;
    AImageCropRect crop{};
    if (AImage_getHardwareBuffer(image, &buffer) || AImage_getCropRect(image, &crop) || !buffer) return false;
    // API32's PRIVATE ImageReader may report a zero-origin image crop even
    // though the producer's per-buffer MediaCodec format carries padding.
    // Use that buffer's actual codec crop, matched by presentation timestamp.
    crop = codecCrop;
    AHardwareBuffer_Desc desc{};
    AHardwareBuffer_describe(buffer, &desc);
    static void *nativewindow = dlopen("libnativewindow.so", RTLD_NOW);
    static auto nativeHandle = nativewindow ? reinterpret_cast<const int *(*)(const AHardwareBuffer *)>(
            dlsym(nativewindow, "AHardwareBuffer_getNativeHandle")) : nullptr;
    const int *handle = nativeHandle ? nativeHandle(buffer) : nullptr;
    // native_handle_t's public native-handle ABI starts with three ints.
    const int *data = handle;
    if (!handle || data[1] < 1 || crop.left < 0 || crop.top < 0 ||
            crop.right <= crop.left || crop.bottom <= crop.top ||
            crop.right > int(desc.width) || crop.bottom > int(desc.height)) return false;
    unsigned width = crop.right - crop.left, height = crop.bottom - crop.top;
    struct stat allocation{};
    long long allocationBytes = fstat(data[3], &allocation) == 0 ? allocation.st_size : -1;
    printf("IMAGE alloc=%ux%u stride=%u format=0x%x usage=0x%llx allocation_bytes=%lld crop=%d,%d,%d,%d\n",
           desc.width, desc.height, desc.stride, desc.format, (unsigned long long)desc.usage,
           allocationBytes, crop.left, crop.top, crop.right, crop.bottom);
    // This probe requires the actual ten-bit vendor AFBC allocation. It must
    // fail rather than silently accepting a linear/eight-bit fallback.
    bool ten = bits == 10;
    if (desc.format != (ten ? 0x1bu : 0x1au) || (width & 1) || (height & 1)) return false;
    int rgaFormat = ten ? RK_FORMAT_YCbCr_420_SP_10B : RK_FORMAT_YCbCr_420_SP;
    unsigned stride = ((width + 63) & ~63u) * (ten ? 2 : 1);
    unsigned rows = (height + 15) & ~15u;
    MppBufferGroup group = nullptr;
    MppBuffer linear = nullptr;
    if (mpp_buffer_group_get_internal(&group, MPP_BUFFER_TYPE_ION | MPP_BUFFER_FLAGS_CACHABLE) ||
            mpp_buffer_get(group, &linear, size_t(stride) * rows * 3 / 2)) return false;
    unsigned sourceStride = desc.stride ? desc.stride : desc.width;
    auto source = wrapbuffer_fd(data[3], desc.width, desc.height,
                               rgaFormat, int(sourceStride), int(desc.height));
    auto target = wrapbuffer_fd(mpp_buffer_get_fd(linear), width, height,
                               rgaFormat, int(stride), int(rows));
    source.rd_mode = IM_AFBC16x16_MODE;
    target.rd_mode = IM_RASTER_MODE;
    im_rect from{crop.left, crop.top, int(width), int(height)}, to{0, 0, int(width), int(height)}, empty{};
    rga_buffer_t unused{};
    IM_STATUS status = improcess(source, target, unused, from, to, empty, IM_SYNC);
    bool ok = status == IM_STATUS_SUCCESS;
    if (!ok) printf("RGA error %d: %s\n", int(status), imStrError(status));
    if (ok) ok = mpp_buffer_sync_ro_begin(linear) == MPP_OK;
    if (ok) {
        auto bytes = static_cast<const uint8_t *>(mpp_buffer_get_ptr(linear));
        for (unsigned plane = 0; plane < 3 && ok; ++plane) {
            unsigned h = plane ? height / 2 : height, w = plane ? width / 2 : width;
            auto base = bytes + (plane ? stride * rows : 0);
            for (unsigned y = 0; y < h && ok; ++y) for (unsigned x = 0; x < w; ++x) {
                unsigned index = plane ? 2 * x + plane - 1 : x;
                uint16_t value = ten ? uint16_t(base[y * stride + index * 2]) |
                                      uint16_t(base[y * stride + index * 2 + 1]) << 8 : base[y * stride + index];
                if (fwrite(&value, 2, 1, output) != 1) { ok = false; break; }
            }
        }
        mpp_buffer_sync_ro_end(linear);
    }
    mpp_buffer_put(linear);
    mpp_buffer_group_put(group);
    return ok;
}

int main(int argc, char **argv) {
    setbuf(stdout, nullptr);
    if (argc != 4 && argc != 5) { puts("Usage: texture_precision CLIP FRAME_LIMIT RAW_OUTPUT [SOURCE_BITS=10]"); return 2; }
    int limit = atoi(argv[2]);
    int bits = argc == 5 ? atoi(argv[4]) : 10;
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
    AImageReader *reader = nullptr;
    if (AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_PRIVATE,
            AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, 16, &reader)) return 6;
    ANativeWindow *window = nullptr;
    if (AImageReader_getWindow(reader, &window)) return 7;
    AMediaCodec *codec = AMediaCodec_createCodecByName("c2.rk.av1.decoder");
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
