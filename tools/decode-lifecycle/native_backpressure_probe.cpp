// SPDX-License-Identifier: MIT
// Async Surface output hold/resume census. Does not control the user's player.
// Holding real MediaCodec output indices prevents ImageReader frame-dropping
// from being mistaken for decoder buffer exhaustion. All fences stay in use.
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkImageReader.h>
#include <android/hardware_buffer.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
struct Output { int32_t index; AMediaCodecBufferInfo info; };
struct State {
    std::mutex mutex;
    std::deque<int32_t> inputs;
    std::deque<Output> outputs;
    int errors = 0, formats = 0;
};
static double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static void input(AMediaCodec*, void *ctx, int32_t index) {
    auto &s = *static_cast<State*>(ctx); std::lock_guard<std::mutex> lock(s.mutex);
    s.inputs.push_back(index);
}
static void output(AMediaCodec*, void *ctx, int32_t index, AMediaCodecBufferInfo *info) {
    auto &s = *static_cast<State*>(ctx); std::lock_guard<std::mutex> lock(s.mutex);
    s.outputs.push_back({index, *info});
}
static void format(AMediaCodec*, void *ctx, AMediaFormat*) {
    auto &s = *static_cast<State*>(ctx); std::lock_guard<std::mutex> lock(s.mutex); ++s.formats;
}
static void error(AMediaCodec*, void *ctx, media_status_t code, int32_t action, const char *detail) {
    auto &s = *static_cast<State*>(ctx); std::lock_guard<std::mutex> lock(s.mutex); ++s.errors;
    printf("CODEC_ERROR code=%d action=%d detail=%s\n", code, action, detail ? detail : "");
}
int main(int argc, char **argv) {
    if (argc != 4 && argc != 6) return 2;
    setbuf(stdout, nullptr);
    const int count = atoi(argv[2]), holdSeconds = atoi(argv[3]);
    if (count < 100 || count > 600 || holdSeconds < 5 || holdSeconds > 15) return 2;
    int fd = open(argv[1], O_RDONLY); struct stat st{};
    if (fd < 0 || fstat(fd, &st)) return 3;
    auto *ex = AMediaExtractor_new();
    if (AMediaExtractor_setDataSourceFd(ex, fd, 0, st.st_size)) return 4;
    AMediaFormat *f = nullptr; const char *mime = nullptr;
    for (size_t t = 0; t < AMediaExtractor_getTrackCount(ex); ++t) {
        auto *candidate = AMediaExtractor_getTrackFormat(ex, t);
        if (AMediaFormat_getString(candidate, "mime", &mime) && !strcmp(mime, "video/hevc")) {
            f = candidate; AMediaExtractor_selectTrack(ex, t); break;
        }
        AMediaFormat_delete(candidate);
    }
    if (!f) return 5;
    int32_t width = 0, height = 0;
    AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_HEIGHT, &height);
    AImageReader *reader = nullptr; ANativeWindow *window = nullptr;
    // Composer usage exercises native NV15 rather than forcing P010 copy-back.
    constexpr uint64_t composerUsage = 1ULL << 11;
    if (AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_PRIVATE,
            composerUsage | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, 8, &reader) ||
            AImageReader_getWindow(reader, &window)) return 6;
    if (argc == 6) {
        const int initialWidth = atoi(argv[4]), initialHeight = atoi(argv[5]);
        if (initialWidth < 64 || initialHeight < 64 || initialWidth > 8192 || initialHeight > 8192) return 2;
        AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_WIDTH, initialWidth);
        AMediaFormat_setInt32(f, AMEDIAFORMAT_KEY_HEIGHT, initialHeight);
    }
    State s;
    auto *codec = AMediaCodec_createCodecByName("c2.rk.hevc.decoder");
    if (!codec) return 7;
    AMediaCodecOnAsyncNotifyCallback cb{input, output, format, error};
    if (AMediaCodec_setAsyncNotifyCallback(codec, cb, &s) ||
            AMediaCodec_configure(codec, f, window, nullptr, 0) || AMediaCodec_start(codec)) return 8;
    int packets = 0, outputs = 0, images = 0, held = 0, errors = 0;
    bool inEos = false, outEos = false;
    std::set<int64_t> expected, actual;
    double begin = now(), holdBegin = 0;
    while (now() - begin < holdSeconds + 20 && !outEos && !errors) {
        int32_t ix = -1;
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            if (s.errors) { errors += s.errors; break; }
            if (!inEos && !s.inputs.empty()) { ix = s.inputs.front(); s.inputs.pop_front(); }
            if (!s.outputs.empty() && !holdBegin) holdBegin = now();
            if (holdBegin && now() - holdBegin < holdSeconds && int(s.outputs.size()) > held)
                held = s.outputs.size();
        }
        if (ix >= 0) {
            size_t cap = 0; auto *data = AMediaCodec_getInputBuffer(codec, ix, &cap);
            const auto n = packets < count ? AMediaExtractor_readSampleData(ex, data, cap) : -1;
            const auto pts = AMediaExtractor_getSampleTime(ex); inEos = n < 0;
            if (AMediaCodec_queueInputBuffer(codec, ix, 0, inEos ? 0 : n, inEos ? 0 : pts,
                    inEos ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0)) ++errors;
            else if (!inEos) { ++packets; expected.insert(pts); AMediaExtractor_advance(ex); }
        }
        if (holdBegin && now() - holdBegin >= holdSeconds) {
            std::deque<Output> ready;
            { std::lock_guard<std::mutex> lock(s.mutex); ready.swap(s.outputs); }
            for (auto &o : ready) {
                const bool eos = o.info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
                const bool frame = !(o.info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) && (!eos || o.info.size > 0);
                if (frame) { ++outputs; if (!actual.insert(o.info.presentationTimeUs).second) ++errors; }
                if (AMediaCodec_releaseOutputBuffer(codec, o.index, frame)) ++errors;
                outEos |= eos;
            }
            AImage *image = nullptr;
            while (AImageReader_acquireNextImage(reader, &image) == AMEDIA_OK) {
                ++images; AImage_delete(image); image = nullptr;
            }
        }
        usleep(1000);
    }
    const double stopBegin = now(); const auto stop = AMediaCodec_stop(codec);
    const double stopMs = (now() - stopBegin) * 1000;
    AMediaCodec_delete(codec);
    bool pass = held >= 8 && packets == count && outputs == count && expected == actual &&
            outEos && !errors && !s.errors && !stop && s.formats;
    printf("RESULT pass=%d held=%d hold_seconds=%d packets=%d outputs=%d pts_exact=%d eos=%d errors=%d formats=%d images=%d stop_ms=%.3f\n",
            pass, held, holdSeconds, packets, outputs, expected == actual, outEos, errors + s.errors, s.formats, images, stopMs);
    AImageReader_delete(reader); AMediaFormat_delete(f); AMediaExtractor_delete(ex); close(fd);
    return pass ? 0 : 10;
}
