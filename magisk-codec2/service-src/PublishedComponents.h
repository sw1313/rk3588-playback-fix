// SPDX-License-Identifier: Apache-2.0
// Retain the complete hash-pinned stock factory registry. The candidate library
// has a smaller map intentionally; it must never define the service registry.
#pragma once
#include "C2RKMediaUtils.h"
static const ComponentMapEntry kPublishedComponents[] = {
    { "c2.rk.avc.decoder", MPP_VIDEO_CodingAVC, MEDIA_MIMETYPE_VIDEO_AVC, MPP_CTX_DEC },
    { "c2.rk.vp9.decoder", MPP_VIDEO_CodingVP9, MEDIA_MIMETYPE_VIDEO_VP9, MPP_CTX_DEC },
    { "c2.rk.hevc.decoder", MPP_VIDEO_CodingHEVC, MEDIA_MIMETYPE_VIDEO_HEVC, MPP_CTX_DEC },
    { "c2.rk.vp8.decoder", MPP_VIDEO_CodingVP8, MEDIA_MIMETYPE_VIDEO_VP8, MPP_CTX_DEC },
    { "c2.rk.mpeg2.decoder", MPP_VIDEO_CodingMPEG2, MEDIA_MIMETYPE_VIDEO_MPEG2, MPP_CTX_DEC },
    { "c2.rk.m4v.decoder", MPP_VIDEO_CodingMPEG4, MEDIA_MIMETYPE_VIDEO_MPEG4, MPP_CTX_DEC },
    { "c2.rk.h263.decoder", MPP_VIDEO_CodingH263, MEDIA_MIMETYPE_VIDEO_H263, MPP_CTX_DEC },
    { "c2.rk.av1.decoder", MPP_VIDEO_CodingAV1, MEDIA_MIMETYPE_VIDEO_AV1, MPP_CTX_DEC },
    { "c2.rk.avc.decoder.secure", MPP_VIDEO_CodingAVC, MEDIA_MIMETYPE_VIDEO_AVC, MPP_CTX_DEC },
    { "c2.rk.vp9.decoder.secure", MPP_VIDEO_CodingVP9, MEDIA_MIMETYPE_VIDEO_VP9, MPP_CTX_DEC },
    { "c2.rk.hevc.decoder.secure", MPP_VIDEO_CodingHEVC, MEDIA_MIMETYPE_VIDEO_HEVC, MPP_CTX_DEC },
    { "c2.rk.vp8.decoder.secure", MPP_VIDEO_CodingVP8, MEDIA_MIMETYPE_VIDEO_VP8, MPP_CTX_DEC },
    { "c2.rk.mpeg2.decoder.secure", MPP_VIDEO_CodingMPEG2, MEDIA_MIMETYPE_VIDEO_MPEG2, MPP_CTX_DEC },
    { "c2.rk.m4v.decoder.secure", MPP_VIDEO_CodingMPEG4, MEDIA_MIMETYPE_VIDEO_MPEG4, MPP_CTX_DEC },
    { "OMX.c2.rk.avc.decoder", MPP_VIDEO_CodingAVC, MEDIA_MIMETYPE_VIDEO_AVC, MPP_CTX_DEC },
    { "OMX.c2.rk.vp9.decoder", MPP_VIDEO_CodingVP9, MEDIA_MIMETYPE_VIDEO_VP9, MPP_CTX_DEC },
    { "OMX.c2.rk.hevc.decoder", MPP_VIDEO_CodingHEVC, MEDIA_MIMETYPE_VIDEO_HEVC, MPP_CTX_DEC },
    { "c2.rk.avc.encoder", MPP_VIDEO_CodingAVC, MEDIA_MIMETYPE_VIDEO_AVC, MPP_CTX_ENC },
    { "c2.rk.hevc.encoder", MPP_VIDEO_CodingHEVC, MEDIA_MIMETYPE_VIDEO_HEVC, MPP_CTX_ENC },
    { "c2.rk.vp8.encoder", MPP_VIDEO_CodingVP8, MEDIA_MIMETYPE_VIDEO_VP8, MPP_CTX_ENC },
    { "OMX.c2.rk.av1.decoder", MPP_VIDEO_CodingAV1, MEDIA_MIMETYPE_VIDEO_AV1, MPP_CTX_DEC },
    { "c2.rx.avc.decoder", MPP_VIDEO_CodingAVC, MEDIA_MIMETYPE_VIDEO_AVC, MPP_CTX_DEC },
    { "c2.rx.hevc.decoder", MPP_VIDEO_CodingHEVC, MEDIA_MIMETYPE_VIDEO_HEVC, MPP_CTX_DEC },
    { "c2.rx.vp9.decoder", MPP_VIDEO_CodingVP9, MEDIA_MIMETYPE_VIDEO_VP9, MPP_CTX_DEC },
    { "c2.rx.av1.decoder", MPP_VIDEO_CodingAV1, MEDIA_MIMETYPE_VIDEO_AV1, MPP_CTX_DEC }
};
