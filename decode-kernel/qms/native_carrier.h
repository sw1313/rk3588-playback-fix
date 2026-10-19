/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_CARRIER_H
#define RKQMS_NATIVE_CARRIER_H
/* Exact-image carrier preparation. ALL native planes affected by this CRTC
 * must participate: factory atomic_begin rebuilds the live plane assignment.
 * Caller owns native modeset locks and verifies the full function contract.
 * This header performs check/preparation only, never commits or writes MMIO.
 */
#include "native_atomic.h"
extern int drm_atomic_add_affected_planes(struct drm_atomic_state *, struct drm_crtc *);
extern int drm_atomic_check_only(struct drm_atomic_state *);

static inline int rkqms_native_carrier_locked(struct drm_device *drm, struct drm_crtc *crtc,
                                             struct drm_atomic_state **out)
{
    struct drm_atomic_state *state;
    unsigned long clone;
    unsigned char changes, flags;
    int ret = rkqms_native_prepare_locked(drm, crtc, &state);
    *out = NULL;
    if (ret) return ret;
    ret = drm_atomic_add_affected_planes(state, crtc);
    if (ret) goto fail;
    ret = drm_atomic_check_only(state);
    if (ret) goto fail;
    /* The native getter returns the existing transaction clone. No second
     * allocation, no live-state mutation. Allow plane changes but never a
     * mode/active/connector change, an async commit or ALLOW_MODESET.
     */
    clone = (unsigned long)drm_atomic_get_crtc_state(state, crtc);
    if (IS_ERR_OR_NULL((void *)clone) ||
        copy_from_kernel_nofault(&changes, (void *)(clone + 0xa), 1) ||
        copy_from_kernel_nofault(&flags, (char *)state + 0x10, 1) ||
        (changes & 0x0e) || (flags & 0x05)) {
        ret = -ESTALE; goto fail;
    }
    *out = state;
    return 0;
fail:
    rkqms_native_state_put(state);
    return ret;
}
#endif
