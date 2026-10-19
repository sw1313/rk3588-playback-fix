// SPDX-License-Identifier: MIT
// Load a candidate core in this process; observe configuration before any input.
// No mounts, persistent settings, video submission or player counters are changed.
#include <C2ComponentFactory.h>
#include <C2Config.h>
#include <dlfcn.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <mutex>
using namespace android;

struct Sink : C2Component::Listener {
    std::mutex mutex;
    std::condition_variable ready;
    int updates = 0, frames = 0, completions = 0, errors = 0;
    uint32_t notifiedDelay = 0;
    void onTripped_nb(std::weak_ptr<C2Component>, std::vector<std::shared_ptr<C2SettingResult>>) override {
        std::lock_guard<std::mutex> lock(mutex); ++errors; ready.notify_all();
    }
    void onError_nb(std::weak_ptr<C2Component>, uint32_t) override {
        std::lock_guard<std::mutex> lock(mutex); ++errors; ready.notify_all();
    }
    void onWorkDone_nb(std::weak_ptr<C2Component>, std::list<std::unique_ptr<C2Work>> works) override {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto &work : works) {
            if (work->result != C2_OK || work->workletsProcessed != 1 || work->worklets.size() != 1) {
                ++errors; continue;
            }
            const auto &output = work->worklets.front()->output;
            frames += output.buffers.size();
            if (!(output.flags & C2FrameData::FLAG_INCOMPLETE)) ++completions;
            if (output.flags != C2FrameData::FLAG_INCOMPLETE ||
                    work->input.ordinal.frameIndex.peeku() != uint64_t(INT64_MAX) ||
                    !work->input.buffers.empty() || !output.buffers.empty() ||
                    output.configUpdate.size() != 1) ++errors;
            for (const auto &param : output.configUpdate) {
                auto delay = C2PortActualDelayTuning::output::From(param.get());
                if (!delay || !delay->value) { ++errors; continue; }
                notifiedDelay = delay->value; ++updates;
            }
        }
        ready.notify_all();
    }
};

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    setbuf(stdout, nullptr);
    const bool enabled = atoi(argv[3]) != 0;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { printf("LOAD_FAIL %s\n", dlerror()); return 3; }
    auto create = reinterpret_cast<C2ComponentFactory *(*)(std::string)>(dlsym(library, "CreateRKCodec2Factory"));
    auto destroy = reinterpret_cast<void (*)(C2ComponentFactory *)>(dlsym(library, "DestroyRKCodec2Factory"));
    if (!create || !destroy) return 4;
    auto factory = create(argv[2]);
    if (!factory) return 5;
    std::shared_ptr<C2Component> component;
    if (factory->createComponent(1, &component) != C2_OK || !component) return 6;
    auto sink = std::make_shared<Sink>();
    if (component->setListener_vb(sink, C2_MAY_BLOCK) != C2_OK) return 7;
    C2StreamPictureSizeInfo::output size(0u, 1920, 1080);
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    if (component->intf()->config_vb({&size}, C2_MAY_BLOCK, &failures) != C2_OK) return 8;
    if (component->start() != C2_OK) return 9;
    {
        std::unique_lock<std::mutex> lock(sink->mutex);
        sink->ready.wait_for(lock, std::chrono::milliseconds(enabled ? 2000 : 100),
                [&] { return sink->updates || sink->errors; });
    }
    C2PortActualDelayTuning::output delay(0);
    if (component->intf()->query_vb({&delay}, {}, C2_MAY_BLOCK, nullptr) != C2_OK) return 10;
    auto stopped = component->stop();
    auto released = component->release();
    component.reset(); destroy(factory);
    const bool passed = stopped == C2_OK && released == C2_OK && delay.value > 0 &&
            sink->updates == int(enabled) && !sink->frames && !sink->completions && !sink->errors &&
            (!enabled || sink->notifiedDelay == delay.value);
    printf("RESULT pass=%d enabled=%d queried_delay=%u updates=%d notified_delay=%u frames=%d input_completions=%d errors=%d\n",
            passed, enabled, delay.value, sink->updates, sink->notifiedDelay,
            sink->frames, sink->completions, sink->errors);
    return passed ? 0 : 11;
}
