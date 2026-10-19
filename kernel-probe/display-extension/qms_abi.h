/* SPDX-License-Identifier: GPL-2.0-only */
/* Exact-image metadata inspection by default; RKQMS_CARRIER adds explicitly
 * selected native commit/recovery/scoped register experiments. Never QMS.
 */
#include <linux/regmap.h>
#include <uapi/drm/drm_mode.h>
#include "core.c"
#include "native_atomic.h"
extern void drm_modeset_lock_all(struct drm_device *);
extern void drm_modeset_unlock_all(struct drm_device *);
extern void drm_mode_convert_to_umode(struct drm_mode_modeinfo *, const void *);
static int qms_regmap_spinlock, qms_regmap_val_bytes, qms_regmap_stride;
static int qms_base_modes, qms_private_state_backend, qms_vop_version;
static int qms_atomic_clone_roundtrips;
module_param(qms_regmap_spinlock, int, 0444);
module_param(qms_regmap_val_bytes, int, 0444);
module_param(qms_regmap_stride, int, 0444);
module_param(qms_base_modes, int, 0444);
module_param(qms_private_state_backend, int, 0444);
module_param(qms_vop_version, int, 0444);
module_param(qms_atomic_clone_roundtrips, int, 0444);
#ifdef RKQMS_CARRIER
#include "transaction.c"
#include "session.c"
#include "lifecycle.c"
#include "qms_carrier.h"
#endif

static int inspect_qms_abi(unsigned long drm, unsigned long qp, unsigned long vop,
                           struct device *hdmi_device, struct device *vop_device)
{
    unsigned long map, lock, unlock, data, parent;
    struct rkqms_caps caps = { 0x3ffe, 1, 1 };
    unsigned int i;
    int ret = 0;
    BUILD_BUG_ON(sizeof(struct drm_mode_modeinfo) != 68);
    if (!read_word(qp + 0xbe0, &map) || !map ||
        !read_word(vop + 8, &parent) || parent != (unsigned long)vop_device ||
        !read_word(map + 0x20, &lock) || !read_word(map + 0x28, &unlock) ||
        !read_word(vop + 0x2858, &data) || data != anchor + DISPLAY_VOP_DATA_DELTA)
        return -ESTALE;
    if (regmap_get_device((void *)map) != hdmi_device)
        return -ESTALE;
    qms_regmap_val_bytes = regmap_get_val_bytes((void *)map);
    qms_regmap_stride = regmap_get_reg_stride((void *)map);
    qms_regmap_spinlock = lock == anchor + DISPLAY_REGMAP_LOCK_DELTA &&
                         unlock == anchor + DISPLAY_REGMAP_UNLOCK_DELTA;
    if (copy_from_kernel_nofault(&qms_vop_version, (void *)vop, 4) || qms_vop_version != 0x4017 ||
        qms_regmap_val_bytes != 4 || qms_regmap_stride != 4)
        return -ESTALE;
    /* Serialization uses native functions, never an inferred DRM mutex layout. */
    drm_modeset_lock_all((void *)drm);
    for (i = 0; i < 4; ++i) {
        struct drm_crtc *crtc = drm_crtc_from_index((void *)drm, i);
        unsigned long state, owner;
        struct drm_mode_modeinfo mode;
        struct rkqms_mode m;
        struct rkqms_state model;
        unsigned int vic;
        if (!crtc || !read_word((unsigned long)crtc + 0x668, &owner) || owner != vop ||
            !read_word((unsigned long)crtc + 0x5c0, &state) || !state) {
            ret = -ESTALE; break;
        }
        memset(&mode, 0, sizeof(mode));
        drm_mode_convert_to_umode(&mode, (void *)(state + 0x18));
        if (!mode.clock) continue;
        memset(&m, 0, sizeof(m));
        m.clock_khz = mode.clock; m.flags = mode.flags;
        m.width = mode.hdisplay; m.height = mode.vdisplay;
        m.htotal = mode.htotal; m.vtotal = mode.vtotal;
        m.vsync_start = mode.vsync_start; m.vsync_end = mode.vsync_end;
        for (vic = 0; vic < 6; ++vic) {
            static const unsigned char vics[] = {4, 47, 16, 63, 97, 102};
            if (!rkqms_init(&model, &m, vics[vic], &caps, 1)) {
                struct drm_atomic_state *temporary;
                unsigned long after;
                ret = rkqms_native_prepare_locked((void *)drm, crtc, &temporary);
                if (ret) break;
                rkqms_native_state_put(temporary);
                if (!read_word((unsigned long)crtc + 0x5c0, &after) || after != state) {
                    ret = -ESTALE; break;
                }
                ++qms_atomic_clone_roundtrips;
#ifdef RKQMS_CARRIER
                ret = check_native_carrier(drm, crtc, state, qp, vop, &m);
                if (ret) break;
#endif
                ++qms_base_modes;
                qms_private_state_backend = 1;
                break;
            }
        }
        if (ret) break;
    }
    drm_modeset_unlock_all((void *)drm);
    if (!ret)
#ifdef RKQMS_CARRIER
        pr_info("RK_QMS_ABI: regmap spinlock=%d val_bytes=%d stride=%d base_modes=%d; selected scoped native experiments completed, QMS off\n",
#else
        pr_info("RK_QMS_ABI: regmap spinlock=%d val_bytes=%d stride=%d base_modes=%d; core-only; no display/register changes\n",
#endif
                qms_regmap_spinlock, qms_regmap_val_bytes, qms_regmap_stride, qms_base_modes);
    return ret;
}
