/* SPDX-License-Identifier: MIT
 * Retry only the block pool's C2_BLOCKING result. Cancellation is a lifecycle
 * request, never a timeout or permission to skip an allocator/fence operation.
 */
#pragma once
#include <C2.h>
#include <chrono>
#include <thread>

template<class Fetch, class Canceled>
c2_status_t rkCancelablePoolFetch(bool enabled, Fetch fetch, Canceled canceled) {
    if (!enabled) {
        c2_status_t status;
        do { status = fetch(); } while (status == C2_BLOCKING);
        return status;
    }
    for (;;) {
        if (canceled()) return C2_BAD_STATE;
        const auto begin = std::chrono::steady_clock::now();
        const c2_status_t status = fetch();
        if (status != C2_BLOCKING) return status;
        {
            if (canceled()) return C2_BAD_STATE;
            // BufferQueue already sleeps on retry in this API32 framework.
            // Back off only fast-returning pools; do not add a full extra delay.
            const auto spent = std::chrono::steady_clock::now() - begin;
            const auto minimum = std::chrono::milliseconds(1);
            if (spent < minimum) std::this_thread::sleep_for(minimum - spent);
        }
    }
}
