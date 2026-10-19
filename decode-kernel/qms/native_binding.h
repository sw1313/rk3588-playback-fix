/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_BINDING_H
#define RKQMS_NATIVE_BINDING_H
#include "controller.h"
#include "native_engine.h"
#include "native_lifecycle.h"
#include "native_link.h"
struct rkqms_native_binding {
    struct drm_device *drm;
    struct rkqms_registers *registers;
    struct rkqms_native_lifecycle *life;
    struct rkqms_controller *controller;
    struct rkqms_native_link *link;
    unsigned long original_helper,helper[16],commit_mutex;
    void (*original_flush)(struct drm_crtc *,struct drm_crtc_state *);
    struct rkqms_native_step *step;
    struct task_struct *task;
    struct drm_atomic_state *transaction;
    bool attached;
};
/* Initialize only from exact-image contract under retained native object
 * references. Code/tables are pinned until reboot; no hot-unload promise.
 */
int rkqms_binding_prepare(struct rkqms_native_binding *,unsigned long expected_tail_helpers,
                         unsigned long expected_tail,unsigned long expected_flush);
int rkqms_binding_frame(void *,struct rkqms_session *);
int rkqms_binding_recover(void *,struct rkqms_session *);
int rkqms_binding_pm_quiesce(void *);
int rkqms_binding_validate(struct rkqms_native_binding *,const struct rkqms_session *);
#endif
