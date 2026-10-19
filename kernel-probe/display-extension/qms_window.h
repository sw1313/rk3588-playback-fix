/* SPDX-License-Identifier: GPL-2.0-only */
/* Scope: a single unchanged factory carrier under native all-modeset locks.
 * Publication/restoration also own the factory commit-tail mutex. Its sole
 * atomic_flush dispatch loads the helper pointer INSIDE that mutex. A future
 * functional cold hook must stay pinned; this scoped experiment is not a
 * general hot-unload implementation. Optional echo writes ORIGINAL register
 * values only; OFF session observation does not write registers.
 */
#include <linux/interrupt.h>
#include "native_registers.h"
#include "native_engine.h"
#include "native_pm.h"
static bool window_once;
static bool register_echo;
static bool session_observe;
static bool lifecycle_inspect;
static int lifecycle_prepared,pm_prepared;
static struct rkqms_native_lifecycle inspect_lifecycle;
static struct rkqms_native_pm inspect_pm;
static struct rkqms_session window_session;
static struct rkqms_native_step window_step;
static int session_published;
static struct rkqms_registers window_registers;
static int register_bound, register_begun, register_packets, register_timing;
static int register_finished, register_error, register_initial_vtotal;
static int window_hits, window_owner_ok, window_mutex_ok, window_no_irq;
static int window_foreign, window_returns, window_restored;
static unsigned long window_helper[16], window_original_helper, window_mutex;
static struct drm_atomic_state *window_transaction;
static struct task_struct *window_task;
static void (*window_original_flush)(struct drm_crtc *, struct drm_crtc_state *);
module_param(window_once, bool, 0444);
module_param(register_echo, bool, 0444);
module_param(session_observe, bool, 0444);
module_param(lifecycle_inspect, bool, 0444);
module_param(lifecycle_prepared, int, 0444);
module_param(pm_prepared, int, 0444);
module_param(session_published, int, 0444);
module_param(register_bound, int, 0444);
module_param(register_begun, int, 0444);
module_param(register_packets, int, 0444);
module_param(register_timing, int, 0444);
module_param(register_finished, int, 0444);
module_param(register_error, int, 0444);
module_param(register_initial_vtotal, int, 0444);
module_param(window_hits, int, 0444);
module_param(window_owner_ok, int, 0444);
module_param(window_mutex_ok, int, 0444);
module_param(window_no_irq, int, 0444);
module_param(window_foreign, int, 0444);
module_param(window_returns, int, 0444);
module_param(window_restored, int, 0444);

static void carrier_flush_window(struct drm_crtc *crtc, struct drm_crtc_state *old)
{
    unsigned long owner = 0, mutex_owner = 0;
    if (current == window_task && read_word((unsigned long)old + 0x148, &owner) &&
        owner == (unsigned long)window_transaction) {
        ++window_hits; window_owner_ok = 1;
        if (read_word(window_mutex, &mutex_owner) &&
            (mutex_owner & ~7UL) == (unsigned long)current) window_mutex_ok = 1;
        if (!in_interrupt()) window_no_irq = 1;
        if (session_observe) {
            rkqms_native_step_window(&window_step,crtc,old);
        } else if (register_echo) {
            struct rkqms_registers *r=&window_registers;
            unsigned int i;
            r->commit_mutex=window_mutex;r->old=old;r->transaction=window_transaction;
            r->task=current;r->generation=1;r->sequence=drm_crtc_vblank_count(crtc);
            r->flush=window_original_flush;
            register_error=rkqms_registers_begin(r,1,r->sequence);
            if (!register_error) {
                register_begun=1;register_initial_vtotal=r->saved_timing>>16;
                for (i=0;i<8&&!register_error;++i) {
                    u32 value, after;
                    register_error=regmap_read(r->map,0xd00+i*4,&value);
                    if (!register_error) register_error=rkqms_registers_packet(r,0xd00+i*4,value);
                    if (!register_error) register_error=regmap_read(r->map,0xd00+i*4,&after);
                    if (!register_error && after!=value) register_error=-EIO;
                    if (!register_error) ++register_packets;
                }
                if (!register_error) {
                    register_error=rkqms_registers_timing(r,r->base.vtotal,r->base.vsync_start-r->base.height);
                    if (!register_error) register_timing=1;
                }
                if (!register_error) register_error=rkqms_registers_enable(r,BIT(9),0);
                rkqms_registers_end(r);
            } else window_original_flush(crtc,old);
        } else window_original_flush(crtc, old);
        ++window_returns;
    } else {
        ++window_foreign;
        window_original_flush(crtc, old);
    }
}

static int inspect_never_quiesce(void *p){(void)p;return -EOPNOTSUPP;}
static int carrier_register_bind(struct drm_crtc *crtc, unsigned long qp, unsigned long vop,
                                  const struct rkqms_mode *base)
{
    static const long table_deltas[]={DISPLAY_VP_REGS0_DELTA,DISPLAY_VP_REGS1_DELTA,
                                      DISPLAY_VP_REGS2_DELTA,DISPLAY_VP_REGS3_DELTA};
    unsigned char port;
    int ret;
    if (!register_echo && !session_observe && !lifecycle_inspect) return 0;
    if (register_echo && session_observe) return -EINVAL;
    if (lifecycle_inspect && (window_once || register_echo || session_observe || commit_once)) return -EINVAL;
    if ((!lifecycle_inspect && !window_once) || copy_from_kernel_nofault(&port,(char *)crtc+0x688,1) || port>3) return -EINVAL;
    ret=rkqms_registers_bind(&window_registers,crtc,qp,vop,anchor+DISPLAY_VOP_DATA_DELTA,
                            anchor+table_deltas[port],base);
    if (!ret) register_bound=1;
    if (!ret && lifecycle_inspect) {
        unsigned long device;
        const struct display_table_contract *bridge=&display_tables[DISPLAY_QP_BRIDGE];
        const struct display_table_contract *pm=&display_tables[DISPLAY_HDMI_PM];
        if (!read_word(qp+0x828,&device)) return -EFAULT;
        ret=rkqms_native_lifecycle_prepare(&inspect_lifecycle,&window_registers,anchor+bridge->delta,
                   anchor+(long)bridge->slots[6],anchor+(long)bridge->slots[10],anchor+(long)bridge->slots[11]);
        if (ret) return ret;
        lifecycle_prepared=1;
        ret=rkqms_native_pm_prepare(&inspect_pm,&inspect_lifecycle,(void *)device,anchor+pm->delta,
                   anchor+(long)pm->slots[2],anchor+(long)pm->slots[3],inspect_never_quiesce,NULL);
        if (!ret) {pm_prepared=1;ret=rkqms_native_pm_discard_unpublished(&inspect_pm);}
        if (rkqms_native_lifecycle_discard_unpublished(&inspect_lifecycle)) return -EBUSY;
        return ret;
    }
    if (!ret && session_observe) {
        struct rkqms_status id={0};
        struct rkqms_caps caps={0x7fe,1,1};
        struct rkqms_io io={&window_registers,rkqms_registers_begin,rkqms_registers_packet,
                            rkqms_registers_enable,rkqms_registers_timing,rkqms_registers_end};
        unsigned char vic=base->width==3840?97:base->width==4096?102:base->width==1920?16:4;
        /* INTERNAL OFF-only fixture identity. This is NOT current link/HDR/
         * startup proof and never exposed to HWC or any endpoint. No submit
         * is allowed: test the real transaction's publication barrier only.
         */
        id.abi=RKQMS_ABI;id.bytes=sizeof(id);id.generation=1;id.link_cookie=1;
        id.connector=1;id.crtc=1;id.proof=RKQMS_PROOF_ALL;id.bits_per_component=8;
        ret=rkqms_session_init(&window_session,base,vic,&caps,&io,&id);
    }
    return ret;
}

static int carrier_window_attach(unsigned long drm, struct drm_crtc *crtc, struct drm_atomic_state *state)
{
    unsigned long private, helpers, tail, original;
    if (!read_word(drm + 0x48, &private) || !private ||
        !read_word(drm + 0x5e0, &helpers) || helpers != anchor + DISPLAY_NATIVE_TAIL_HELPERS_DELTA ||
        !read_word(helpers, &tail) || tail != anchor + DISPLAY_NATIVE_TAIL_DELTA ||
        !read_word((unsigned long)crtc + 0x1b0, &original) ||
        original != anchor + display_tables[DISPLAY_CRTC_HELPER].delta ||
        copy_from_kernel_nofault(window_helper, (void *)original, sizeof(window_helper)) ||
        window_helper[12] != anchor + DISPLAY_NATIVE_FLUSH_DELTA) return -ESTALE;
    window_mutex = private + 0x218;
    window_original_helper = original;
    window_original_flush = (void *)window_helper[12];
    window_helper[12] = (unsigned long)carrier_flush_window;
    window_transaction = state; window_task = current;
    if (session_observe)
        rkqms_native_step_prepare(&window_step,&window_session,&window_registers,state,
                                  window_mutex,window_original_flush);
    mutex_lock((void *)window_mutex);
    if (READ_ONCE(*(unsigned long *)((char *)crtc + 0x1b0)) != original) {
        mutex_unlock((void *)window_mutex); return -ESTALE;
    }
    smp_store_release((unsigned long *)((char *)crtc + 0x1b0), (unsigned long)window_helper);
    mutex_unlock((void *)window_mutex);
    return 0;
}

static void carrier_window_detach(struct drm_crtc *crtc)
{
    /* No pointer can be dispatched while this mutex is held. Previous native
     * tails can still clean up states, but all future flush dispatches reload
     * the restored permanent factory helper table after taking this mutex.
     */
    mutex_lock((void *)window_mutex);
    smp_store_release((unsigned long *)((char *)crtc + 0x1b0), window_original_helper);
    window_restored = READ_ONCE(*(unsigned long *)((char *)crtc + 0x1b0)) == window_original_helper;
    mutex_unlock((void *)window_mutex);
    window_task = NULL; window_transaction = NULL;
}
