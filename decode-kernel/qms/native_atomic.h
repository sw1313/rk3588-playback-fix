/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_ATOMIC_H
#define RKQMS_NATIVE_ATOMIC_H
/* EXACT original Image ABI, guarded by generated instruction/table contracts.
 * Native drm_modeset_lock_all() owns and publishes the acquire context at
 * drm_device+0x1f0. Native drm_atomic_get_crtc_state() reads state+0x48 and
 * writes its new clone's owner at +0x148. These offsets were checked against
 * the original functions, not inferred from a newer DRM struct definition.
 * Caller holds native all-modeset locks for the complete object's lifetime.
 * This constructor never invokes atomic_check/commit or writes hardware.
 */
#include <linux/kref.h>
#include <linux/err.h>
#include <linux/uaccess.h>
struct drm_device;
struct drm_crtc;
struct drm_atomic_state;
struct drm_crtc_state;
extern struct drm_atomic_state *drm_atomic_state_alloc(struct drm_device *);
extern struct drm_crtc_state *drm_atomic_get_crtc_state(struct drm_atomic_state *, struct drm_crtc *);
extern void __drm_atomic_state_free(struct kref *);

static inline void rkqms_native_state_put(struct drm_atomic_state *state)
{
    /* Native state_init sets ref at offset0. All subsequent destruction and
     * factory private-state/blob reference cleanup uses the native release.
     */
    kref_put((struct kref *)state, __drm_atomic_state_free);
}

static inline int rkqms_native_prepare_locked(struct drm_device *drm, struct drm_crtc *crtc,
                                              struct drm_atomic_state **out)
{
    struct drm_atomic_state *state;
    struct drm_crtc_state *clone;
    unsigned long context, owner;
    unsigned char flags;
    *out = NULL;
    if (copy_from_kernel_nofault(&context, (char *)drm + 0x1f0, sizeof(context)) || !context)
        return -ESTALE;
    state = drm_atomic_state_alloc(drm);
    if (!state) return -ENOMEM;
    /* Only the NEWLY ALLOCATED transaction is changed. Never change the live
     * CRTC state or its mode and never enable ALLOW_MODESET for a QMS frame.
     */
    flags = READ_ONCE(*((unsigned char *)state + 0x10));
    WRITE_ONCE(*((unsigned char *)state + 0x10), flags & ~1U);
    WRITE_ONCE(*(void **)((char *)state + 0x48), (void *)context);
    clone = drm_atomic_get_crtc_state(state, crtc);
    if (IS_ERR_OR_NULL(clone)) {
        int error = clone ? PTR_ERR(clone) : -ENOMEM;
        rkqms_native_state_put(state);
        return error;
    }
    if (copy_from_kernel_nofault(&owner, (char *)clone + 0x148, sizeof(owner)) || owner != (unsigned long)state) {
        rkqms_native_state_put(state);
        return -ESTALE;
    }
    *out = state;
    return 0;
}
#endif
