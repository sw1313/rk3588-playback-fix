// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <cutils/native_handle.h>
struct eden_gralloc_layout {
    uint32_t fourcc;
    uint64_t modifier, allocation, offset, pitch;
    int fd;
};
#ifdef __cplusplus
extern "C" {
#endif
int eden_query_gralloc(const native_handle_t*, struct eden_gralloc_layout*);
#ifdef __cplusplus
}
#endif
