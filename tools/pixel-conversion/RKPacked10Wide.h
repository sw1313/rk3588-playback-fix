// SPDX-License-Identifier: MIT
#pragma once
#include "RKPacked10.h"

// Gather adjacent byte pairs directly into halfword lanes. Each block reads
// exactly the same twenty source bytes as the original sixteen-sample path.
// The overlapping loads deliberately stop at byte 19, including on the last
// row immediately before a guard page. All ten source bits are retained.
static inline void rkPacked10WideRow(const uint8_t *src, uint8_t *dst, size_t width) {
    size_t x = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    const uint8_t indexA0[8] = {0,1,1,2,2,3,3,4};
    const uint8_t indexA1[8] = {5,6,6,7,7,8,8,9};
    const uint8_t indexB0[8] = {6,7,7,8,8,9,9,10};
    const uint8_t indexB1[8] = {11,12,12,13,13,14,14,15};
    const int16_t shifts[8] = {6,4,2,0,6,4,2,0};
    const uint8x8_t a0 = vld1_u8(indexA0), a1 = vld1_u8(indexA1);
    const uint8x8_t b0 = vld1_u8(indexB0), b1 = vld1_u8(indexB1);
    const int16x8_t shift = vld1q_s16(shifts);
    const uint16x8_t mask = vdupq_n_u16(0xffc0);
    for (; x + 16 <= width; x += 16) {
        const uint8_t *p = src + x * 5 / 4;
        uint8x8x2_t first = {{vld1_u8(p), vld1_u8(p + 8)}};
        uint8x8x2_t second = {{vld1_u8(p + 4), vld1_u8(p + 12)}};
        const uint16x8_t a = vreinterpretq_u16_u8(vcombine_u8(
                vtbl2_u8(first, a0), vtbl2_u8(first, a1)));
        const uint16x8_t b = vreinterpretq_u16_u8(vcombine_u8(
                vtbl2_u8(second, b0), vtbl2_u8(second, b1)));
        vst1q_u16(reinterpret_cast<uint16_t *>(dst + x * 2),
                vandq_u16(vshlq_u16(a, shift), mask));
        vst1q_u16(reinterpret_cast<uint16_t *>(dst + x * 2 + 16),
                vandq_u16(vshlq_u16(b, shift), mask));
    }
#endif
    rkPacked10Row(src + x * 5 / 4, dst + x * 2, width - x, false);
}

static inline bool rkPacked10ToP010Wide(const uint8_t *src, size_t srcSize,
        size_t srcStride, size_t srcHeight, uint8_t *dstY, uint8_t *dstUV,
        size_t yStride, size_t uvStride, size_t width, size_t height,
        bool fast = true) {
    if (!fast) return rkPacked10ToP010(src, srcSize, srcStride, srcHeight,
            dstY, dstUV, yStride, uvStride, width, height, false);
    if (!src || !dstY || !dstUV || !width || !height || (width & 1) || (height & 1) ||
        width > 8192 || height > 8192 || srcHeight < height ||
        srcStride < (width * 10 + 7) / 8 || yStride < width * 2 || uvStride < width * 2 ||
        !srcHeight || srcStride > SIZE_MAX / srcHeight ||
        yStride > SIZE_MAX / height || uvStride > SIZE_MAX / (height / 2)) return false;
    const size_t chromaOffset = srcStride * srcHeight;
    if (chromaOffset > srcSize || srcStride > (srcSize - chromaOffset) / (height / 2)) return false;
    // Retain the existing cacheable row plus sequential destination memcpy:
    // gralloc mappings can be uncached or write-combined. Caller owns fences.
    alignas(64) uint8_t row[8192 * 2];
    for (size_t plane = 0; plane < 2; ++plane) {
        const uint8_t *base = src + (plane ? chromaOffset : 0);
        uint8_t *dst = plane ? dstUV : dstY;
        const size_t stride = plane ? uvStride : yStride;
        for (size_t y = 0; y < (plane ? height / 2 : height); ++y) {
            rkPacked10WideRow(base + y * srcStride, row, width);
            memcpy(dst + y * stride, row, width * 2);
        }
    }
    return true;
}
