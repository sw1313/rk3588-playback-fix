/* SPDX-License-Identifier: GPL-2.0-only */
/* Factory transaction checks; optional ONE unchanged native commit or a
 * baseline full native recovery modeset. Optional scoped flush/register echo
 * and OFF-only session observation. Never activates QMS; not read-only when
 * commit_once=1. Full Image ABI fingerprints required.
 */
#include <linux/ktime.h>
#include "native_carrier.h"
#include "native_recover.h"
extern int drm_atomic_commit(struct drm_atomic_state *);
extern unsigned int drm_crtc_vblank_count(struct drm_crtc *);
static bool commit_once;
static bool recovery_check;
static int recovery_verified;
static int carrier_checks, carrier_planes, carrier_commit_calls, carrier_commit_result;
static int carrier_before_sequence, carrier_after_sequence, carrier_preserved;
static unsigned long carrier_commit_us;
module_param(commit_once, bool, 0444);
module_param(recovery_check, bool, 0444);
module_param(recovery_verified, int, 0444);
module_param(carrier_checks, int, 0444);
module_param(carrier_planes, int, 0444);
module_param(carrier_commit_calls, int, 0444);
module_param(carrier_commit_result, int, 0444);
module_param(carrier_before_sequence, int, 0444);
module_param(carrier_after_sequence, int, 0444);
module_param(carrier_preserved, int, 0444);
module_param(carrier_commit_us, ulong, 0444);
#include "qms_window.h"

static int check_native_carrier(unsigned long drm, struct drm_crtc *crtc, unsigned long live,
                                 unsigned long qp, unsigned long vop, const struct rkqms_mode *base)
{
    struct drm_atomic_state *state;
    unsigned long slots, clones[64], planes[64], old_states[64], fbs[64], crtc_clone;
    struct drm_mode_modeinfo before, after;
    unsigned int plane_count, i, included = 0;
    unsigned char active;
    unsigned long observed_state;
    u64 started;
    struct rkqms_registers recovery_registers;
    u32 recovery_scheduler=0;
    int ret, attached = 0;
    if (window_once && !commit_once) return -EINVAL;
    if (recovery_check && (window_once || register_echo || session_observe)) return -EINVAL;
    if (copy_from_kernel_nofault(&active, (void *)(live + 9), 1) || !active ||
        copy_from_kernel_nofault(&plane_count, (void *)(drm + 0x2e8), 4) ||
        !plane_count || plane_count > ARRAY_SIZE(planes)) return -ESTALE;
    memset(&before, 0, sizeof(before));
    drm_mode_convert_to_umode(&before, (void *)(live + 0x18));
    ret = recovery_check ? rkqms_native_recovery_prepare_locked((void *)drm, crtc, &state) :
                           rkqms_native_carrier_locked((void *)drm, crtc, &state);
    if (ret) return ret;
    if (!read_word((unsigned long)state + 0x18, &slots)) {ret = -EFAULT; goto out;}
    crtc_clone = (unsigned long)drm_atomic_get_crtc_state(state, crtc);
    for (i = 0; i < plane_count; ++i) {
        unsigned long object, old, clone, fb, owner, target;
        if (!read_word(slots + i * 32, &object)) {ret = -EFAULT; goto out;}
        if (!object) continue;
        if (!read_word(slots + i * 32 + 16, &old) || !old ||
            !read_word(slots + i * 32 + 24, &clone) || !clone ||
            !read_word(object + 0x4d8, &observed_state) || observed_state != old ||
            !read_word(old + 8, &target) || target != (unsigned long)crtc ||
            !read_word(old + 16, &fb) || !fb ||
            !read_word(clone + 16, &owner) || owner != fb ||
            !read_word(clone + 8, &target) || target != (unsigned long)crtc) {ret = -ESTALE; goto out;}
        planes[included] = object; old_states[included] = old;
        clones[included] = clone; fbs[included++] = fb;
    }
    if (!included) {ret = -ENODATA; goto out;}
    ++carrier_checks; carrier_planes = included;
    ret=carrier_register_bind(crtc,qp,vop,base);
    if (ret) goto out;
    if (recovery_check) {
        static const long deltas[]={DISPLAY_VP_REGS0_DELTA,DISPLAY_VP_REGS1_DELTA,
                                   DISPLAY_VP_REGS2_DELTA,DISPLAY_VP_REGS3_DELTA};
        unsigned char port;
        if (copy_from_kernel_nofault(&port,(char *)crtc+0x688,1) || port>3) {ret=-ESTALE;goto out;}
        ret=rkqms_registers_bind(&recovery_registers,crtc,qp,vop,anchor+DISPLAY_VOP_DATA_DELTA,
                               anchor+deltas[port],base);
        if (ret) goto out;
        /* QMS was never enabled by this probe: do not take ownership of an
         * already enabled VTEM bit. No custom writes in this recovery test.
         */
        ret=regmap_read(recovery_registers.map,0xaa8,&recovery_scheduler);
        if (ret) goto out;
        if (recovery_scheduler&BIT(9)) {ret=-EBUSY;goto out;}
    }
    if (commit_once) {
        if (window_once) {
            ret = carrier_window_attach(drm, crtc, state);
            if (ret) goto out;
            attached = 1;
        }
        started = ktime_get_ns();
        carrier_before_sequence = drm_crtc_vblank_count(crtc);
        ++carrier_commit_calls;
        ret = drm_atomic_commit(state);
        carrier_commit_result = ret;
        carrier_after_sequence = drm_crtc_vblank_count(crtc);
        carrier_commit_us = (ktime_get_ns() - started) / 1000;
        if (attached) {carrier_window_detach(crtc); attached = 0;}
        if (session_observe) {
            ret=rkqms_native_step_finish(&window_step,ret);
            if (!ret && window_session.have_observation && window_session.status.observed_ns &&
                window_session.status.state==RKQMS_BASE && !window_session.status.serial &&
                window_session.transaction.core.phase==RKQMS_OFF) session_published=1;
            else if (!ret) ret=-ESTALE;
        }
        if (ret) goto out;
        if (window_once && (window_hits != 1 || !window_owner_ok || !window_mutex_ok ||
                            !window_no_irq || window_returns != 1 || !window_restored)) {
            ret = -ESTALE; goto out;
        }
        if (register_echo) {
            if (register_error) {ret=register_error;goto out;}
            ret=rkqms_registers_finish(&window_registers);
            if (ret) {register_error=ret;goto out;}
            register_finished=1;
        }
    }
    if (!read_word((unsigned long)crtc + 0x5c0, &observed_state) ||
        observed_state != (commit_once ? crtc_clone : live)) {ret = -ESTALE; goto out;}
    memset(&after, 0, sizeof(after));
    drm_mode_convert_to_umode(&after, (void *)(observed_state + 0x18));
    if (memcmp(&before, &after, sizeof(before))) {ret = -ESTALE; goto out;}
    for (i = 0; i < included; ++i) {
        unsigned long fb;
        if (!read_word(planes[i] + 0x4d8, &observed_state) ||
            observed_state != (commit_once ? clones[i] : old_states[i]) ||
            !read_word(observed_state + 16, &fb) || fb != fbs[i]) {ret = -ESTALE; goto out;}
    }
    if (commit_once && (unsigned int)(carrier_after_sequence - carrier_before_sequence) >
                       (recovery_check ? 180U : 8U)) {
        ret = -ETIME; goto out;
    }
    carrier_preserved = 1;
    if (recovery_check && commit_once) {
        u32 scheduler,timing;
        unsigned char powered,disabled;
        ret=regmap_read(recovery_registers.map,0xaa8,&scheduler);
        if (ret) goto out;
        timing=readl(recovery_registers.mmio+recovery_registers.timing_offset);
        if (copy_from_kernel_nofault(&powered,(void *)(qp+0xb42),1) || !powered ||
            copy_from_kernel_nofault(&disabled,(void *)(qp+0xb74),1) || disabled ||
            (scheduler&BIT(9)) ||
#ifndef RK_DECODE_DDC_RECOVERY
            scheduler!=recovery_scheduler ||
#endif
            ((timing>>16)&0x1fff)!=base->vtotal ||
            (timing&0xffff)!=(u32)(base->vsync_end-base->vsync_start)) {ret=-ESTALE;goto out;}
        recovery_verified=1;
#ifdef RK_DECODE_DDC_RECOVERY
        /* Corrected EDID may legitimately enable native AVI/audio/HDR packet
         * owners absent in the stale DVI setup. Native mode/buffer/power/
         * timing checks above still apply; QMS/VTEM must remain off. */
        pr_info("RK_DECODE_DDC_RECOVER: native packet scheduler before=%08x after=%08x; mode/buffers preserved; VTEM off\n",
                recovery_scheduler,scheduler);
#endif
    }
out:
    if (attached) carrier_window_detach(crtc);
    /* The carrier allocates only factory states. No live allocation retains
     * any pointer to this research module, including after a native commit.
     */
    rkqms_native_state_put(state);
    if (register_echo) pr_info("RK_QMS_REGISTER: bound=%d begin=%d packets=%d timing=%d finish=%d err=%d; original values only, VTEM TX remains off\n",
        register_bound,register_begun,register_packets,register_timing,register_finished,register_error);
    if (register_echo) pr_info("RK_QMS_REGISTER: diagnostic=%d hw_timing=%08x shadow_timing=%08x scheduler=%08x\n",
        window_registers.diagnostic,window_registers.observed_timing,window_registers.observed_shadow,window_registers.scheduler_before);
    pr_info("RK_QMS_CARRIER: checks=%d planes=%d commit_calls=%d commit_result=%d preserved=%d duration_us=%lu; no QMS timing/VTEM changes\n",
            carrier_checks, carrier_planes, carrier_commit_calls, carrier_commit_result,
            carrier_preserved, carrier_commit_us);
    return ret;
}
