/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_RECOVER_H
#define RKQMS_NATIVE_RECOVER_H
/* Factory full modeset recovery. Caller owns native all-modeset locks, has
 * stopped QMS work and holds object references. No buffer/fence substitution.
 * The exact Image contract must include the invoked native helper paths.
 */
#include "native_atomic.h"
#include "native_registers.h"
extern int drm_atomic_add_affected_planes(struct drm_atomic_state *, struct drm_crtc *);
extern int drm_atomic_add_affected_connectors(struct drm_atomic_state *, struct drm_crtc *);
extern int drm_atomic_commit(struct drm_atomic_state *);
extern int drm_atomic_check_only(struct drm_atomic_state *);

/* Preparation is separable so compatibility tests can run native check without
 * causing a modeset. mode_changed is a bit in the native clone, not live state.
 */
static inline int rkqms_native_recovery_prepare_locked(struct drm_device *drm,
                         struct drm_crtc *crtc, struct drm_atomic_state **out)
{
    struct drm_atomic_state *state;
    struct drm_crtc_state *clone;
    unsigned char flags, changes;
    int ret;
    *out=NULL;
    ret=rkqms_native_prepare_locked(drm,crtc,&state);
    if (ret) return ret;
    clone=drm_atomic_get_crtc_state(state,crtc);
    if (IS_ERR_OR_NULL(clone)) {ret=clone?PTR_ERR(clone):-ENOMEM;goto out;}
    flags=READ_ONCE(*((unsigned char *)state+0x10));
    WRITE_ONCE(*((unsigned char *)state+0x10),flags|1U);
    changes=READ_ONCE(*((unsigned char *)clone+0xa));
    WRITE_ONCE(*((unsigned char *)clone+0xa),changes|2U);
    ret=drm_atomic_add_affected_connectors(state,crtc);
    if (ret) goto out;
    ret=drm_atomic_add_affected_planes(state,crtc);
    if (ret) goto out;
    ret=drm_atomic_check_only(state);
    if (ret) goto out;
    if (!(READ_ONCE(*((unsigned char *)clone+0xa))&2U) ||
        !(READ_ONCE(*((unsigned char *)state+0x10))&1U) ||
        (READ_ONCE(*((unsigned char *)state+0x10))&4U)) {ret=-ESTALE;goto out;}
    *out=state;
    return 0;
out:
    rkqms_native_state_put(state);
    return ret;
}

/* The caller must first disable its owned VTEM while QP is powered, then
 * verify actual base timing, power/link/HDR and VTEM off after this returns.
 * Native bridge enable is void: a zero commit return alone is NOT recovery.
 */
static inline int rkqms_native_recover_locked(struct drm_device *drm, struct drm_crtc *crtc)
{
    struct drm_atomic_state *state;
    int ret=rkqms_native_recovery_prepare_locked(drm,crtc,&state);
    if (ret) return ret;
    ret=drm_atomic_commit(state);
    rkqms_native_state_put(state);
    return ret;
}

/* Does NOT certify link/HDR identity, cancel foreign requests or unpublish a
 * hook. Binding must do those lifetime checks before marking quiescence.
 */
static inline int rkqms_native_restore_owned_locked(struct drm_device *drm,
                                      struct rkqms_registers *registers,bool owns_vtem)
{
    int ret=rkqms_registers_disable_owned_vtem(registers,owns_vtem);
    if (ret) return ret;
    ret=rkqms_native_recover_locked(drm,registers->crtc);
    if (ret) return ret;
    return rkqms_registers_verify_base(registers);
}
#endif
