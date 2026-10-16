// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif

// NV15 is a little-endian stream of ten-bit samples. P010 retains all ten
// bits in the upper bits of each little-endian halfword; no rounding occurs.
static inline void rkPacked10Row(const uint8_t *src, uint8_t *dst,
                                size_t width, bool fast) {
    size_t x = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    if (fast) {
        const uint8_t offsets[16] = {0,1,2,3,5,6,7,8,10,11,12,13,15,16,17,18};
        const int16_t shifts[8] = {0,-2,-4,-6,0,-2,-4,-6};
        const int16x8_t shift = vld1q_s16(shifts);
        const uint16x8_t mask = vdupq_n_u16(1023);
        const uint8x8_t one = vdup_n_u8(1);
        const uint8x8_t idx0 = vld1_u8(offsets), idx1 = vld1_u8(offsets + 8);
        for (; x + 16 <= width; x += 16) {
            const uint8_t *p = src + x * 5 / 4;
            // Exactly twenty bytes per sixteen samples. Never overread a row,
            // even when the next byte is an unmapped page or allocator padding.
            uint32_t tail;
            memcpy(&tail, p + 16, sizeof(tail));
            uint8x8x3_t table;
            table.val[0] = vld1_u8(p);
            table.val[1] = vld1_u8(p + 8);
            table.val[2] = vcreate_u8(tail);
            const uint16x8_t a = vorrq_u16(vmovl_u8(vtbl3_u8(table, idx0)),
                    vshll_n_u8(vtbl3_u8(table, vadd_u8(idx0, one)), 8));
            const uint16x8_t b = vorrq_u16(vmovl_u8(vtbl3_u8(table, idx1)),
                    vshll_n_u8(vtbl3_u8(table, vadd_u8(idx1, one)), 8));
            vst1q_u16(reinterpret_cast<uint16_t *>(dst + x * 2),
                    vshlq_n_u16(vandq_u16(vshlq_u16(a, shift), mask), 6));
            vst1q_u16(reinterpret_cast<uint16_t *>(dst + x * 2 + 16),
                    vshlq_n_u16(vandq_u16(vshlq_u16(b, shift), mask), 6));
        }
    }
#endif
    for (; x < width; ++x) {
        const size_t bit = x * 10, off = bit / 8;
        const uint16_t word = uint16_t(src[off]) | (uint16_t(src[off + 1]) << 8);
        const uint16_t sample = ((word >> (bit % 8)) & 1023) << 6;
        dst[x * 2] = uint8_t(sample);
        dst[x * 2 + 1] = uint8_t(sample >> 8);
    }
}

static inline bool rkPacked10ToP010(const uint8_t *src, size_t srcSize,
        size_t srcStride, size_t srcHeight, uint8_t *dstY, uint8_t *dstUV,
        size_t yStride, size_t uvStride, size_t width, size_t height,
        bool fast = true) {
    if (!src || !dstY || !dstUV || !width || !height || (width & 1) || (height & 1) ||
        width > 8192 || height > 8192 || srcHeight < height ||
        srcStride < (width * 10 + 7) / 8 || yStride < width * 2 || uvStride < width * 2 ||
        !srcHeight || srcStride > SIZE_MAX / srcHeight ||
        yStride > SIZE_MAX / height || uvStride > SIZE_MAX / (height / 2)) return false;
    const size_t chromaOffset = srcStride * srcHeight;
    if (chromaOffset > srcSize || srcStride > (srcSize - chromaOffset) / (height / 2)) return false;
    // Mapped gralloc destinations can be uncached/write-combined. Convert into
    // a cacheable, aligned row, then write sequentially through libc memcpy.
    // Keep mapper lock/unlock and DMA fences with the caller; never skip cache
    // synchronization or change buffer usage/format to obtain a speedup.
    alignas(64) uint8_t row[8192 * 2];
    for (size_t plane = 0; plane < 2; ++plane) {
        const uint8_t *base = src + (plane ? chromaOffset : 0);
        uint8_t *dst = plane ? dstUV : dstY;
        const size_t stride = plane ? uvStride : yStride;
        for (size_t y = 0; y < (plane ? height / 2 : height); ++y) {
            uint8_t *out = dst + y * stride;
            if (fast) {
                rkPacked10Row(base + y * srcStride, row, width, true);
                memcpy(out, row, width * 2);
            } else {
                rkPacked10Row(base + y * srcStride, out, width, false);
            }
        }
    }
    return true;
}
