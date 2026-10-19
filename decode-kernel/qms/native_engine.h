/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_ENGINE_H
#define RKQMS_NATIVE_ENGINE_H
/* A single tagged, blocking native transaction. The caller owns native
 * all-modeset locks, object/vblank references and session serialization. It
 * publishes the scoped flush window before commit and restores it afterwards.
 * This is NOT a worker, ioctl endpoint, permanent hook or link-proof provider.
 */
#include "session.h"
#include <linux/ktime.h>
#include "native_registers.h"
extern u64 drm_crtc_vblank_count_and_time(struct drm_crtc *, ktime_t *);

struct rkqms_native_step {
    struct rkqms_session *live;
    struct rkqms_session staged;
    struct rkqms_registers *registers;
    struct drm_atomic_state *transaction;
    struct task_struct *task;
    unsigned long commit_mutex;
    void (*flush)(struct drm_crtc *, struct drm_crtc_state *);
    int error;
    unsigned int windows;
    rq_u64 initial_revision, initial_serial, initial_generation;
    rq_u32 previous_sequence, sampled_sequence;
    rq_u64 previous_ns, sampled_ns;
    int observation_diagnostic;
    unsigned char reanchored, repeated, latch_reobserved, initial_phase, initial_awaiting_latch;
};

static inline void rkqms_native_step_prepare(struct rkqms_native_step *step,
                       struct rkqms_session *live, struct rkqms_registers *registers,
                       struct drm_atomic_state *transaction, unsigned long commit_mutex,
                       void (*flush)(struct drm_crtc *, struct drm_crtc_state *))
{
    memset(step,0,sizeof(*step));
    step->live=live;step->staged=*live;step->registers=registers;
    step->transaction=transaction;step->commit_mutex=commit_mutex;
    step->flush=flush;step->task=current;
    step->initial_revision=live->transaction.core.revision;
    step->initial_serial=live->status.serial;step->initial_generation=live->status.generation;
    step->initial_phase=live->transaction.core.phase;
    step->initial_awaiting_latch=live->awaiting_latch;
}

static inline void rkqms_native_step_window(struct rkqms_native_step *step,
                         struct drm_crtc *crtc, struct drm_crtc_state *old)
{
    struct rkqms_registers *r=step->registers;
    ktime_t ns=0;
    u64 sequence;
    ++step->windows;
    if (step->windows!=1 || current!=step->task || crtc!=r->crtc) {
        step->error=-ESTALE;
        step->flush(crtc,old);
        return;
    }
    sequence=drm_crtc_vblank_count_and_time(crtc,&ns);
    step->previous_sequence=step->staged.status.sequence;
    step->previous_ns=step->staged.status.observed_ns;
    step->sampled_sequence=(u32)sequence;step->sampled_ns=(u64)ns;
    step->observation_diagnostic=!ns ? 91 :
        step->staged.have_observation && (u64)ns<=step->previous_ns ? 92 :
        step->staged.have_observation && step->staged.transaction.core.phase!=RKQMS_OFF &&
        (u32)(sequence-step->previous_sequence)!=1 ? 93 : 0;
    r->task=current;r->transaction=step->transaction;r->old=old;
    r->commit_mutex=step->commit_mutex;r->flush=step->flush;
    r->sequence=(u32)sequence;r->generation=step->staged.status.generation;
    r->vtem_before=step->staged.transaction.core.vtem_enabled;
    r->flushed=0;r->error=0;
    if (step->staged.constant_reanchor) {
        step->error=rkqms_registers_hold(r,&step->staged.transaction.core);
        if (!step->error)step->error=rkqms_session_anchor_verified(&step->staged,r->generation,r->sequence,(u64)ns);
        if (step->error) {rkqms_registers_end(r);return;}
        step->reanchored=1;
        step->observation_diagnostic=0;
    }
    if (step->observation_diagnostic==93 && rkqms_session_repeatable(&step->staged)) {
        /* Only a monotonic gap in a physically unchanged integer notice.
         * Full packet/shadow/latched timing/power/epoch qualification first;
         * publication and the native completion check below stay unchanged. */
        step->error=rkqms_registers_repeat(r,&step->staged.transaction.core);
        if (!step->error)step->error=rkqms_session_repeat_verified(&step->staged,r->generation,r->sequence,(u64)ns);
        if (step->error) {rkqms_registers_end(r);return;}
        step->repeated=1;step->observation_diagnostic=0;
    }
    if (step->observation_diagnostic==93 && rkqms_session_latch_repeatable(&step->staged)) {
        /* No missing timing update: M_CONST=1 and integer timing were already
         * committed and latched. Requalify the complete live hardware state
         * rather than treating delayed final observation as programming loss.
         * Native begin/flush/fence/finish and staged publication stay intact. */
        step->error=rkqms_registers_hold(r,&step->staged.transaction.core);
        if (!step->error)step->error=rkqms_session_latch_observe_verified(&step->staged,r->generation,r->sequence,(u64)ns);
        if (step->error) {rkqms_registers_end(r);return;}
        step->latch_reobserved=1;step->observation_diagnostic=0;
    }
    if (!step->latch_reobserved)
        step->error=rkqms_session_frame(&step->staged,r->generation,r->sequence,(u64)ns);
    /* OFF observations do not call the abstract I/O. Still establish the real
     * native register/power window and check its unchanged baseline before
     * the single delegated flush; never certify an unobserved transaction.
     */
    if (!step->error && !r->flushed) step->error=rkqms_registers_begin(r,r->generation,r->sequence);
    rkqms_registers_end(r);
}

static inline int rkqms_native_step_finish(struct rkqms_native_step *step,int native_error)
{
    int ret=native_error ? native_error : step->error;
    if (!ret && (step->live->transaction.core.revision!=step->initial_revision ||
        step->live->status.generation!=step->initial_generation ||
        step->live->status.serial!=step->initial_serial)) ret=-ESTALE;
    if (!ret && step->windows!=1) ret=-ESTALE;
    if (!ret) ret=rkqms_registers_finish(step->registers);
    if (ret) {
        /* A native check/commit error may follow a partial hardware write.
         * Keep the last confirmed state, invalidate success and demand native
         * recovery. In particular, never publish staged completed_serial.
         */
        rkqms_session_fault(step->live,ret);
        return ret;
    }
    *step->live=step->staged;
    return 0;
}
#endif
