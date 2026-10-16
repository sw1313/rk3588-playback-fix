/* SPDX-License-Identifier: MIT */
/* Translate the firmware's Android 12 PREP ABI only.
 * The caller's struct is not today's MppEncPrepCfg. Unselected fields may
 * contain uninitialized stack data; never read them. */
#include "mpp_enc_cfg.h"
typedef struct {
    RK_U32 change;
    RK_S32 width, height, hor_stride, ver_stride;
    RK_S32 format, color, colorprim, colortrc, range;
    RK_S32 rotation, mirroring, denoise;
    RK_U32 sharpen_enable_y, sharpen_enable_uv;
    RK_S32 sharpen_coef[5], sharpen_div, sharpen_threshold;
} RkA12Prep;

static MPP_RET mpi_a12_prep(MpiImpl *p, MpiCmd cmd, MppParam param)
{
    RkA12Prep *old = (RkA12Prep *)param;
    MppEncCfg cfg = NULL;
    MPP_RET ret;
    RK_U32 mask;
    if (!old) return MPP_ERR_NULL_PTR;
    if (p->type != MPP_CTX_ENC) return MPP_ERR_VALUE;
    /* Enhancement bits have no equivalent in the current config schema.
     * Reject them rather than silently disabling an image operation. */
    if (cmd == MPP_ENC_SET_PREP_CFG && (old->change & ~0x000f0035u))
        return MPP_ERR_VALUE;
    ret = mpp_enc_cfg_init(&cfg);
    if (ret) return ret;
    ret = mpp_control(p->ctx, MPP_ENC_GET_CFG, cfg);
    if (ret) goto done;
    if (cmd == MPP_ENC_GET_PREP_CFG) {
        memset(old, 0, sizeof(*old));
#define A12_GET(field, key) do { ret = mpp_enc_cfg_get_s32(cfg, "prep:" key, &old->field); if (ret) goto done; } while (0)
        A12_GET(width, "width"); A12_GET(height, "height");
        A12_GET(hor_stride, "hor_stride"); A12_GET(ver_stride, "ver_stride");
        A12_GET(format, "format"); A12_GET(color, "colorspace");
        A12_GET(colorprim, "colorprim"); A12_GET(colortrc, "colortrc");
        A12_GET(range, "range"); A12_GET(rotation, "rotation");
        A12_GET(mirroring, "mirroring");
#undef A12_GET
        goto done;
    }
    mask = old->change;
#define A12_SET(field, key) do { ret = mpp_enc_cfg_set_s32(cfg, "prep:" key, old->field); if (ret) goto done; } while (0)
    if (mask & 1) {
        A12_SET(width, "width"); A12_SET(height, "height");
        A12_SET(hor_stride, "hor_stride"); A12_SET(ver_stride, "ver_stride");
    }
    if (mask & 4) A12_SET(format, "format");
    if (mask & 16) A12_SET(rotation, "rotation");
    if (mask & 32) A12_SET(mirroring, "mirroring");
    if (mask & (1u << 16)) A12_SET(range, "range");
    if (mask & (1u << 17)) A12_SET(color, "colorspace");
    if (mask & (1u << 18)) A12_SET(colorprim, "colorprim");
    if (mask & (1u << 19)) A12_SET(colortrc, "colortrc");
#undef A12_SET
    ret = mpp_control(p->ctx, MPP_ENC_SET_CFG, cfg);
done:
    mpp_enc_cfg_deinit(cfg);
    return ret;
}
