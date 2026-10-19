// SPDX-License-Identifier: MIT
// Runs the exact helper used by the component with a controlled exhausted pool.
#include "C2RKPoolRetry.h"
#include <atomic>
#include <cstdio>
#include <initializer_list>
#include <thread>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main() {
    using Clock=std::chrono::steady_clock;
    int checks=0,calls=0;
    CHECK(rkCancelablePoolFetch(true,[&]{++calls;return C2_OK;},[]{return true;})==C2_BAD_STATE);
    CHECK(calls==0);checks+=2;
    for (bool enabled : {false,true}) {
        calls=0;
        CHECK(rkCancelablePoolFetch(enabled,[&]{return ++calls<4?C2_BLOCKING:C2_OK;},[]{return false;})==C2_OK);
        CHECK(calls==4);checks+=2;
        CHECK(rkCancelablePoolFetch(enabled,[]{return C2_NO_MEMORY;},[]{return false;})==C2_NO_MEMORY);++checks;
        CHECK(rkCancelablePoolFetch(enabled,[]{return C2_CORRUPTED;},[]{return false;})==C2_CORRUPTED);++checks;
        std::atomic<bool> cancel{false};std::atomic<int> requests{0};
        auto start=Clock::now();
        std::thread requester([&]{std::this_thread::sleep_for(std::chrono::milliseconds(30));cancel=true;});
        auto status=rkCancelablePoolFetch(enabled,[&]{
            ++requests;
            // A BufferQueue-like backend returns BLOCKING after a 10ms wait.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return Clock::now()-start<std::chrono::milliseconds(220)?C2_BLOCKING:C2_OK;
        },[&]{return cancel.load();});
        auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();requester.join();
        CHECK(status==(enabled?C2_BAD_STATE:C2_OK));++checks;
        CHECK(enabled?elapsed<150:elapsed>=220);++checks;
        std::printf("CASE enabled=%d result=%d elapsed_ms=%.3f pool_requests=%d\n",enabled,status,elapsed,int(requests));
    }
    // A pool that returns BLOCKING immediately must not spin at full CPU.
    auto start=Clock::now();calls=0;
    CHECK(rkCancelablePoolFetch(true,[&]{++calls;return C2_BLOCKING;},[&]{return Clock::now()-start>=std::chrono::milliseconds(25);})==C2_BAD_STATE);
    CHECK(calls<100);checks+=2;
    std::printf("PASS checks=%d fast_pool_requests=%d\n",checks,calls);return 0;
}
