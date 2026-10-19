// SPDX-License-Identifier: GPL-2.0-only
#include "native_binding.h"
#include <linux/printk.h>
#include "native_carrier.h"
#include "native_recover.h"
#include <linux/module.h>
#include <linux/delay.h>
static int owner_frame_errors,owner_last_frame_diagnostic,owner_last_frame_delta;
module_param(owner_frame_errors,int,0444);
module_param(owner_last_frame_diagnostic,int,0444);
module_param(owner_last_frame_delta,int,0444);
static int owner_constant_observations,owner_constant_reanchors;
static int owner_notice_reanchors;
static int owner_latch_reobservations;
static int owner_last_scheduler_before,owner_last_scheduler_observed;
static int owner_boundary_records;
/* Root can arm a finite absolute record limit after boot qualification.
 * Default off: no startup budget is spent before the user's test. The hard
 * per-boot ceiling also bounds accidental oversized sysfs input. */
static int owner_boundary_limit;
module_param(owner_constant_observations,int,0444);
module_param(owner_constant_reanchors,int,0444);
module_param(owner_notice_reanchors,int,0444);
module_param(owner_latch_reobservations,int,0444);
module_param(owner_last_scheduler_before,int,0444);
module_param(owner_last_scheduler_observed,int,0444);
module_param(owner_boundary_records,int,0444);
module_param(owner_boundary_limit,int,0600);
extern void drm_modeset_lock_all(struct drm_device *);
extern void drm_modeset_unlock_all(struct drm_device *);
extern int drm_crtc_vblank_get(struct drm_crtc *);
extern void drm_crtc_vblank_put(struct drm_crtc *);
static struct rkqms_native_binding *binding;

static void flush(struct drm_crtc *crtc,struct drm_crtc_state *old)
{
    struct rkqms_native_binding *b=binding;
    unsigned long owner;
    /* Factory flush dispatch occurs under commit_mutex. Old helper tables
     * and original_flush remain allocated for the whole boot, even for a
     * foreign tail that had already loaded the prepared helper pointer.
     */
    if (current==b->task && crtc==b->registers->crtc &&
        rkqms_reg_ptr((unsigned long)old+0x148,&owner) && owner==(unsigned long)b->transaction)
        rkqms_native_step_window(b->step,crtc,old);
    else b->original_flush(crtc,old);
}

int rkqms_binding_prepare(struct rkqms_native_binding *b,unsigned long expected_helpers,
                         unsigned long expected_tail,unsigned long expected_flush)
{
    unsigned long private,helpers,tail,actual;
    if (!b || binding || !b->registers || !b->life || !b->link ||
        b->link->registers!=b->registers || b->link->drm!=b->drm || !b->link->qualified ||
        b->registers->lifecycle!=&b->life->state ||
        !rkqms_reg_ptr((unsigned long)b->drm+0x48,&private) || !private ||
        !rkqms_reg_ptr((unsigned long)b->drm+0x5e0,&helpers) || helpers!=expected_helpers ||
        !rkqms_reg_ptr(helpers,&tail) || tail!=expected_tail ||
        !rkqms_reg_ptr((unsigned long)b->registers->crtc+0x1b0,&actual) || actual!=b->original_helper ||
        copy_from_kernel_nofault(b->helper,(void *)actual,sizeof(b->helper)) ||
        b->helper[12]!=expected_flush) return -ESTALE;
    b->commit_mutex=private+0x218;b->original_flush=(void *)expected_flush;
    b->helper[12]=(unsigned long)flush;b->step=NULL;b->task=NULL;b->transaction=NULL;b->attached=false;
    binding=b;return 0;
}

static int attach(struct rkqms_native_binding *b,struct drm_atomic_state *state,struct rkqms_native_step *step)
{
    unsigned long *slot=(void *)((char *)b->registers->crtc+0x1b0);
    mutex_lock((void *)b->commit_mutex);
    if (b->attached || READ_ONCE(*slot)!=b->original_helper) {mutex_unlock((void *)b->commit_mutex);return -ESTALE;}
    b->task=current;b->transaction=state;b->step=step;
    rkqms_native_step_prepare(step,step->live,b->registers,state,b->commit_mutex,b->original_flush);
    smp_store_release(slot,(unsigned long)b->helper);b->attached=true;
    mutex_unlock((void *)b->commit_mutex);return 0;
}

static void detach(struct rkqms_native_binding *b)
{
    mutex_lock((void *)b->commit_mutex);
    smp_store_release((unsigned long *)((char *)b->registers->crtc+0x1b0),b->original_helper);
    b->attached=false;b->task=NULL;b->transaction=NULL;b->step=NULL;
    mutex_unlock((void *)b->commit_mutex);
}

int rkqms_binding_frame(void *context,struct rkqms_session *s)
{
    struct rkqms_native_binding *b=context;
    struct rkqms_native_step step={0};
    struct drm_atomic_state *state=NULL;
    int ret,ref=0,native_error=0;
    if (b!=binding || !s) return -EINVAL;
    drm_modeset_lock_all(b->drm);
    ret=rkqms_binding_validate(b,s);
    if (ret) goto out;
    ret=drm_crtc_vblank_get(b->registers->crtc);
    if (ret) goto out;
    ref=1;
    if (rkqms_session_constant(s)) {
        ktime_t ns=0;
        u64 sequence=0;
        mutex_lock((void *)b->commit_mutex);
        ret=rkqms_registers_hold(b->registers,&s->transaction.core);
        if (!ret)sequence=drm_crtc_vblank_count_and_time(b->registers->crtc,&ns);
        if (!ret)ret=rkqms_binding_validate(b,s);
        if (!ret) {
            ret=rkqms_session_hold_verified(s,s->status.generation,(u32)sequence,(u64)ns);
            if (!ret)++owner_constant_observations;
            else if (ret==-EAGAIN)ret=0; /* Same real frame: no state update. */
        }
        mutex_unlock((void *)b->commit_mutex);
        goto out;
    }
    ret=rkqms_native_carrier_locked(b->drm,b->registers->crtc,&state);
    if (ret) goto out;
    /* Bounded transition evidence only: no stable-frame logging/poll worker. */
    b->registers->capture_boundary=owner_boundary_records<READ_ONCE(owner_boundary_limit) &&
        owner_boundary_records<1024 &&
        s->transaction.core.phase!=RKQMS_STABLE;
    memset(&b->registers->boundary_before,0,sizeof(b->registers->boundary_before));
    memset(&b->registers->boundary_after,0,sizeof(b->registers->boundary_after));
    b->registers->boundary_before.error=b->registers->boundary_after.error=-ENODATA;
    memset(&b->registers->mode_write,0,sizeof(b->registers->mode_write));
    memset(&b->registers->rate_write,0,sizeof(b->registers->rate_write));
    memset(&step,0,sizeof(step));step.live=s;
    ret=attach(b,state,&step);
    if (ret) goto out;
    ret=drm_atomic_commit(state);
    native_error=ret;
    detach(b);
    if (!ret) ret=rkqms_binding_validate(b,s);
    /* Never publish staged status if the link/HDR/epoch changed during the
     * native commit, even when registers and the native return code look OK.
     */
    ret=rkqms_native_step_finish(&step,ret);
    if (b->registers->capture_boundary && step.windows==1) {
        const struct rkqms_boundary_sample *p=&b->registers->boundary_before;
        const struct rkqms_boundary_sample *q=&b->registers->boundary_after;
        ++owner_boundary_records;
        pr_info("RK_QMS_BOUNDARY: record=%d serial=%llu phase=%u sequence=%u completed=%u result=%d timing=%x/%x scheduler=%x/%x sample_error=%d/%d gcp=%x/%x hdcp2=%x/%x gpio=%x/%x vtem_mode=%x/%x vtem_rate=%x/%x\n",
            owner_boundary_records,(unsigned long long)s->status.serial,step.initial_phase,
            step.sampled_sequence,b->registers->completion_sequence,ret,
            b->registers->saved_timing,b->registers->completion_timing,
            b->registers->scheduler_before,b->registers->observed_scheduler,
            p->error,q->error,p->gcp_control,q->gcp_control,
            p->hdcp2_config,q->hdcp2_config,p->hdcp2_gpio,q->hdcp2_gpio,
            p->packet_mode,q->packet_mode,p->packet_rate,q->packet_rate);
        pr_info("RK_QMS_SCANOUT: record=%d serial=%llu phase=%u dsp=%x/%x vact=%x/%x vcnt=%u/%u mode_ns=%llu mode_seq=%u mode_line=%u rate_ns=%llu rate_seq=%u rate_line=%u\n",
            owner_boundary_records,(unsigned long long)s->status.serial,step.initial_phase,
            p->dsp_control,q->dsp_control,p->vact,q->vact,p->vcnt,q->vcnt,
            (unsigned long long)b->registers->mode_write.ns,b->registers->mode_write.sequence,b->registers->mode_write.vcnt,
            (unsigned long long)b->registers->rate_write.ns,b->registers->rate_write.sequence,b->registers->rate_write.vcnt);
    }
    if (!ret && step.reanchored)++owner_constant_reanchors;
    if (!ret && step.repeated)++owner_notice_reanchors;
    if (!ret && step.latch_reobserved)++owner_latch_reobservations;
out:
    if(ret) {
        ++owner_frame_errors;owner_last_frame_diagnostic=b->registers->diagnostic;
        owner_last_frame_delta=(u32)(b->registers->completion_sequence-b->registers->sequence);
        owner_last_scheduler_before=b->registers->scheduler_before;
        owner_last_scheduler_observed=b->registers->observed_scheduler;
        pr_info("RK_QMS_FRAME: error=%d diagnostic=%d phase=%u sequence=%u epoch=%llu delta=%d expected_timing=%x completed_timing=%x scheduler_before=%x scheduler_observed=%x\n",
            ret,b->registers->diagnostic,s->transaction.core.phase,s->status.sequence,
            (unsigned long long)b->registers->expected_epoch,owner_last_frame_delta,
            b->registers->expected_timing,b->registers->completion_timing,
            (unsigned int)owner_last_scheduler_before,(unsigned int)owner_last_scheduler_observed);
        pr_info("RK_QMS_OBSERVATION: gate=%d native_error=%d step_error=%d windows=%u previous=%u sampled=%u previous_ns=%llu sampled_ns=%llu phase_before=%u awaiting_before=%u repeated=%u\n",
            step.observation_diagnostic,native_error,step.error,step.windows,
            step.previous_sequence,step.sampled_sequence,
            (unsigned long long)step.previous_ns,(unsigned long long)step.sampled_ns,
            step.initial_phase,step.initial_awaiting_latch,step.repeated);
    }
    if (state) rkqms_native_state_put(state);
    if (ref) drm_crtc_vblank_put(b->registers->crtc);
    drm_modeset_unlock_all(b->drm);
    /* The controller owns watchdog cadence and command wakeup. */
    return ret;
}

int rkqms_binding_recover(void *context,struct rkqms_session *s)
{
    struct rkqms_native_binding *b=context;
    struct drm_atomic_state *state=NULL;
    bool foreign;
    int ret;
    if (b!=binding || !s) return -EINVAL;
    drm_modeset_lock_all(b->drm);
    if (s->transaction.core.phase==RKQMS_OFF && !b->life->state.owns_vtem &&
        !b->life->state.dirty && !rkqms_native_link_verify_clean(b->link)) {ret=0;goto out;}
    /* The native bridge may already have completed power-off/reset while a
     * frame was in flight. Its retained ownership/dirty state, VTEM bit and
     * current native timing determine clean quiescence; no empty inactive
     * carrier or restoration of an old foreign mode is required in that case.
     */
    mutex_lock((void *)(b->registers->qp+0xb48));
    foreign=b->life->state.blocked || b->life->state.epoch!=b->registers->expected_epoch;
    mutex_unlock((void *)(b->registers->qp+0xb48));
    if (foreign && !b->life->state.owns_vtem && !b->life->state.dirty &&
        !rkqms_native_link_verify_clean(b->link)) {ret=0;goto out;}
    /* Fence-safe carrier first drains earlier native work. A foreign mode
     * must not be overwritten with an old QMS baseline during recovery.
     */
    ret=rkqms_native_carrier_locked(b->drm,b->registers->crtc,&state);
    if (ret) goto out;
    ret=drm_atomic_commit(state);
    if (ret) goto out;
    mutex_lock((void *)(b->registers->qp+0xb48));
    foreign=b->life->state.blocked || b->life->state.epoch!=b->registers->expected_epoch;
    mutex_unlock((void *)(b->registers->qp+0xb48));
    if (!foreign) {
        ret=rkqms_native_restore_owned_locked(b->drm,b->registers,b->life->state.owns_vtem);
        if (ret) goto out;
    }
    ret=rkqms_native_link_verify_clean(b->link);
    if (!ret) {
        mutex_lock((void *)(b->registers->qp+0xb48));
        rkqms_lifecycle_clear_result(&b->life->state,0);
        rkqms_lifecycle_invalidate(&b->life->state,RKQMS_LIFE_MODE);
        mutex_unlock((void *)(b->registers->qp+0xb48));
    }
out:
    if (state) rkqms_native_state_put(state);
    drm_modeset_unlock_all(b->drm);return ret;
}

int rkqms_binding_pm_quiesce(void *context)
{
    struct rkqms_native_binding *b=context;
    struct rkqms_status status;
    struct rkqms_command c={0};
    int ret;
    if (!b || !b->controller) return -ENODEV;
    ret=rkqms_controller_status(b->controller,&status);
    if (ret) return ret;
    if (status.state==RKQMS_QUIESCENT && !status.proof && !status.link_cookie && !status.error) return 0;
    if (status.state==RKQMS_BASE && !status.error && status.completed_serial==status.serial &&
        status.effective_tfr==status.base_tfr) return 0;
    c.abi=RKQMS_ABI;c.bytes=sizeof(c);c.generation=status.generation;c.link_cookie=status.link_cookie;
    c.connector=status.connector;c.crtc=status.crtc;c.expected_serial=status.serial;c.timeout_ms=1000;
    ret=rkqms_controller_command(b->controller,&c,1);
    if (ret) return ret;
    return c.result.state==RKQMS_BASE && !c.result.error && c.result.completed_serial==c.result.serial &&
        c.result.effective_tfr==c.result.base_tfr ? 0 : -EIO;
}
