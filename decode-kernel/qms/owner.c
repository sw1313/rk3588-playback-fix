// SPDX-License-Identifier: GPL-2.0-only
/* Exact-board cold owner. Publication pins this module and native references
 * until reboot. Only the Decode pre-exec loader may load this component.
 * Empty native DRM file list/open-count is checked under its factory mutex;
 * the loader independently verifies all three stopped, unexecuted init children.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include "owner.h"
#include "native_binding.h"
#include "native_pm.h"
#include "native_carrier.h"
#include "native_recover.h"
#include "endpoint.h"
#include "display_contract.h"
extern void drm_dev_get(struct drm_device *);
extern void drm_dev_put(struct drm_device *);
extern void drm_modeset_lock_all(struct drm_device *);
extern void drm_modeset_unlock_all(struct drm_device *);
extern int drm_atomic_commit(struct drm_atomic_state *);
extern struct drm_crtc *drm_crtc_from_index(struct drm_device *,int);
static struct rkqms_registers registers;
static struct rkqms_native_link link;
static struct rkqms_native_lifecycle life;
static struct rkqms_native_pm pm;
static struct rkqms_native_binding binding;
static struct rkqms_controller controller;
static struct device *retained[4];
static bool cold_bind;
static int owner_online,owner_error,owner_refreshes;
static int owner_phase,owner_inspect_result,owner_width,owner_height,owner_vtotal,owner_clock,owner_active;
static int owner_clients=-1,owner_connector_crtc,owner_crtc_match;
static int owner_link_step,owner_native_edid_present,owner_bus_format,owner_pixel_clock,owner_tmds_clock;
static int owner_phy_clock,owner_native_clean=-EAGAIN,owner_factory_check=-EAGAIN;
static int owner_sync_attempted,owner_sync_result=-EAGAIN,owner_phy_before_sync;
#if !defined(RKQMS_OWNER_INSPECT) || defined(RKQMS_OWNER_SYNC_INSPECT)
#ifdef RKQMS_OWNER_SYNC_INSPECT
static bool sync_native_clock=true;
#else
static bool sync_native_clock;
#endif
module_param(sync_native_clock,bool,0400);
#endif
module_param(cold_bind,bool,0400);
module_param(owner_online,int,0444);
module_param(owner_error,int,0444);
module_param(owner_refreshes,int,0444);
module_param(owner_phase,int,0444);
module_param(owner_inspect_result,int,0444);
module_param(owner_width,int,0444);
module_param(owner_height,int,0444);
module_param(owner_vtotal,int,0444);
module_param(owner_clock,int,0444);
module_param(owner_active,int,0444);
module_param(owner_clients,int,0444);
module_param(owner_connector_crtc,int,0444);
module_param(owner_crtc_match,int,0444);
module_param(owner_link_step,int,0444);
module_param(owner_native_edid_present,int,0444);
module_param(owner_bus_format,int,0444);
module_param(owner_pixel_clock,int,0444);
module_param(owner_tmds_clock,int,0444);
module_param(owner_phy_clock,int,0444);
module_param(owner_native_clean,int,0444);
module_param(owner_factory_check,int,0444);
module_param(owner_sync_attempted,int,0444);
module_param(owner_sync_result,int,0444);
module_param(owner_phy_before_sync,int,0444);
static bool published,boot_verified;
static const u8 base_vics[]={4,47,16,63,97,102};

/* Read-only eligibility check before DDC or a carrier commit. Ordinary native
 * 24/30Hz modes are not QMS base-rate modes and must not trigger a repeated
 * qualification/commit loop while Android is using them. */
static int base_supported(const struct rkqms_mode *mode)
{
    struct rkqms_caps caps={.tfr_mask=0x3ffe,.qms=1,.unchanged_link=1};
    struct rkqms_state state;
    unsigned int i;
    for(i=0;i<ARRAY_SIZE(base_vics);++i)
        if(!rkqms_init(&state,mode,base_vics[i],&caps,1)) return 0;
    return -EOPNOTSUPP;
}

bool rkqms_owner_published(void){return published;}

static int new_session(struct rkqms_session *s,u64 generation,u64 qualified_epoch)
{
    struct rkqms_caps caps={0};
    struct rkqms_status id={0};
    struct rkqms_io io={0};
    struct rkqms_state check;
    unsigned int i,tfr;int ret=-EOPNOTSUPP;u64 epoch;
    if (!generation || !boot_verified || !link.qualified || READ_ONCE(life.pm_blocked)) return -EOPNOTSUPP;
    for(i=0;i<ARRAY_SIZE(base_vics);++i) {
        caps.qms=1;caps.unchanged_link=1;caps.tfr_mask=0;
        for(tfr=1;tfr<=13;++tfr) {
            const struct rkqms_timing *timing=rkqms_timing(base_vics[i],tfr);
            if(timing && timing->vtotal>=link.baseline.mode.vtotal &&
                timing->vtotal+(timing->bits?1:0)<=0x1fff && (link.sink.tfr_mask&BIT(tfr)))
                caps.tfr_mask|=BIT(tfr);
        }
        ret=rkqms_init(&check,&link.baseline.mode,base_vics[i],&caps,generation);
        if (!ret) break;
    }
    if(ret) return ret;
    mutex_lock((void *)(registers.qp+0xb48));
    epoch=qualified_epoch;
    ret=READ_ONCE(life.pm_blocked) ? -EAGAIN : rkqms_lifecycle_arm(&life.state,epoch);
    if(!ret) registers.expected_epoch=epoch;
    mutex_unlock((void *)(registers.qp+0xb48));
    if(ret) return ret;
    registers.base=link.baseline.mode;registers.expected_timing=0;registers.generation=generation;
    io.context=&registers;io.begin=rkqms_registers_begin;io.end=rkqms_registers_end;
    io.write_packet=rkqms_registers_packet;io.update_packet_enable=rkqms_registers_enable;
    io.commit_timing=rkqms_registers_timing;
    id.abi=RKQMS_ABI;id.bytes=sizeof(id);id.generation=generation;id.link_cookie=link.cookie;
    id.connector=link.baseline.connector;id.crtc=link.baseline.crtc;
    /* These are prerequisites, not a claim that the sink has accepted a rate
     * change. Native ABI/window/vblank are verified before endpoint exposure;
     * physical acceptance follows a completed real SWITCH ticket separately.
     */
    id.proof=RKQMS_PROOF_ALL;id.bits_per_component=link.baseline.bpc;
    id.colorspace=link.baseline.colorspace;id.eotf=link.baseline.eotf;
    return rkqms_session_init(s,&registers.base,base_vics[i],&caps,&io,&id);
}

static int refresh(void *p,struct rkqms_session *s)
{
    struct drm_atomic_state *drain=NULL;
    struct rkqms_link_snapshot observed;
    u64 generation,epoch;int ret;
    (void)p;
    if(!published || READ_ONCE(life.pm_blocked)) return -EAGAIN;
    drm_modeset_lock_all(binding.drm);
    if(s->status.state==RKQMS_BASE && !rkqms_binding_validate(&binding,s)) {ret=0;goto out;}
    /* Only OFF/clean native state can be requalified here. Active or dirty
     * state belongs to the frame/recovery worker, never to an idle refresh.
     */
    if(life.state.owns_vtem || life.state.dirty || registers.held) {ret=-EBUSY;goto out;}
    ret=rkqms_native_link_verify_clean(&link);if(ret) goto out;
    if(s->status.state!=RKQMS_QUIESCENT) rkqms_session_quiesced(s);
    generation=s->status.generation;
    ret=rkqms_native_link_observe(&link,&observed);if(ret) {ret=-EAGAIN;goto out;}
    ret=base_supported(&observed.mode);if(ret) goto out;
    ret=rkqms_native_link_qualify(&link);if(ret) {ret=-EAGAIN;goto out;}
    /* Fence-safe blocking carrier drains all earlier native transactions.
     * No timing/packet writes in this step. Qualify again after the drain.
     */
    ret=rkqms_native_carrier_locked(binding.drm,registers.crtc,&drain);if(ret) goto out;
    ret=drm_atomic_commit(drain);if(ret) goto out;
    mutex_lock((void *)(registers.qp+0xb48));epoch=life.state.epoch;mutex_unlock((void *)(registers.qp+0xb48));
    ret=rkqms_native_link_qualify(&link);if(ret) {ret=-EAGAIN;goto out;}
    ret=new_session(s,generation,epoch);
    if(!ret) ++owner_refreshes;
out:
    if(drain) rkqms_native_state_put(drain);
    drm_modeset_unlock_all(binding.drm);
    if(ret==-EBUSY) ret=-EAGAIN;
    owner_error=ret==-EAGAIN ? 0 : ret;
    return ret;
}

/* Shared preparation. The standard inspection build is strictly read-only.
 * Cold functional/sync builds may explicitly request one factory unchanged
 * modeset for the observed zero clock cache, while filelist_mutex excludes
 * native clients. Every link check is repeated afterwards. No QMS writes or
 * callback/endpoint publication take place here. */
static int prepare_owner(unsigned long anchor,struct device **devices,unsigned long drm,
                         unsigned long hdmi,unsigned long qp,unsigned long vop,
                         bool *life_prepared,bool *pm_prepared)
{
    static const long vp_tables[]={DISPLAY_VP_REGS0_DELTA,DISPLAY_VP_REGS1_DELTA,
                                  DISPLAY_VP_REGS2_DELTA,DISPLAY_VP_REGS3_DELTA};
    struct drm_crtc *crtc=NULL;
    struct drm_mode_modeinfo mode;
    struct rkqms_mode base={0};
    unsigned long cs,target;
    u8 active=0,port;unsigned int i;int ret=-ESTALE;
    owner_phase=3;
    if(!rkqms_reg_ptr(qp+0x6d8,&cs) || !rkqms_reg_ptr(cs+8,&target)) goto out;
    owner_connector_crtc=!!target;
    for(i=0;i<4;++i) {
        crtc=drm_crtc_from_index((void *)drm,i);
        if((unsigned long)crtc==target) break;
    }
    owner_phase=4;
    owner_crtc_match=i<4;
    if(i==4 || !rkqms_reg_ptr(target+0x5c0,&cs) ||
        copy_from_kernel_nofault(&active,(void *)(cs+9),1)) goto out;
    owner_active=active;
    if(!active ||
        copy_from_kernel_nofault(&port,(void *)(target+0x688),1) || port>3) goto out;
    memset(&mode,0,sizeof(mode));drm_mode_convert_to_umode(&mode,(void *)(cs+0x18));
    base.clock_khz=mode.clock;base.flags=mode.flags;base.width=mode.hdisplay;base.height=mode.vdisplay;
    base.htotal=mode.htotal;base.vtotal=mode.vtotal;base.vsync_start=mode.vsync_start;base.vsync_end=mode.vsync_end;
    owner_phase=5;
    owner_width=base.width;owner_height=base.height;owner_vtotal=base.vtotal;owner_clock=base.clock_khz;
    ret=rkqms_registers_bind(&registers,crtc,qp,vop,anchor+DISPLAY_VOP_DATA_DELTA,
                            anchor+vp_tables[port],&base);if(ret) goto out;
    owner_phase=6;
    ret=rkqms_native_link_prepare(&link,(void *)drm,&registers,hdmi,devices[3],
                                  (void *)(anchor+DISPLAY_DDC_BLOCK_DELTA));if(ret) goto out;
    owner_phase=7;
    ret=rkqms_native_link_qualify(&link);
#if !defined(RKQMS_OWNER_INSPECT) || defined(RKQMS_OWNER_SYNC_INSPECT)
    if(sync_native_clock && owner_clients==0 &&
       rkqms_native_link_clock_uninitialized(&link,ret)) {
        /* No guessing of PLL state, clk_set_rate or direct cache writes.
         * Use the original DRM/bridge/PHY initialization and keep its planes,
         * buffers and fences. A failed check/commit never becomes proof. */
        owner_phy_before_sync=link.baseline.phy_clock;
        owner_sync_attempted=1;owner_phase=70;
        ret=rkqms_native_link_verify_clean(&link);
        if(!ret)ret=rkqms_native_recover_locked((void *)drm,crtc);
        owner_sync_result=ret;if(ret) goto out;
        owner_phase=7;ret=rkqms_native_link_qualify(&link);
    }
#endif
    if(ret) goto out;
    owner_phase=8;
    ret=rkqms_native_link_verify_clean(&link);if(ret) goto out;
    owner_phase=9;
    ret=rkqms_native_lifecycle_prepare(&life,&registers,anchor+display_tables[DISPLAY_QP_BRIDGE].delta,
        anchor+(long)display_tables[DISPLAY_QP_BRIDGE].slots[6],
        anchor+(long)display_tables[DISPLAY_QP_BRIDGE].slots[10],
        anchor+(long)display_tables[DISPLAY_QP_BRIDGE].slots[11]);if(ret) goto out;
    *life_prepared=true;
    binding.drm=(void *)drm;binding.registers=&registers;binding.life=&life;binding.link=&link;binding.controller=&controller;
    owner_phase=10;
    if(!rkqms_reg_ptr(target+0x1b0,&binding.original_helper)) {ret=-ESTALE;goto out;}
    owner_phase=11;
    ret=rkqms_binding_prepare(&binding,anchor+DISPLAY_NATIVE_TAIL_HELPERS_DELTA,
                             anchor+DISPLAY_NATIVE_TAIL_DELTA,anchor+DISPLAY_NATIVE_FLUSH_DELTA);if(ret) goto out;
    owner_phase=12;
    ret=rkqms_native_pm_prepare(&pm,&life,devices[1],anchor+display_tables[DISPLAY_HDMI_PM].delta,
        anchor+(long)display_tables[DISPLAY_HDMI_PM].slots[2],
        anchor+(long)display_tables[DISPLAY_HDMI_PM].slots[3],rkqms_binding_pm_quiesce,&binding);if(ret) goto out;
    *pm_prepared=true;
    owner_phase=13;
out:
    return ret;
}

#ifdef RKQMS_OWNER_INSPECT
int rkqms_owner_inspect(unsigned long anchor,struct device **devices,unsigned long drm,
                       unsigned long hdmi,unsigned long qp,unsigned long vop)
{
    bool life_prepared=false,pm_prepared=false;
    int ret;u32 clients;struct drm_atomic_state *check=NULL;
    mutex_lock((void *)(drm+0xd0));
    if(!copy_from_kernel_nofault(&clients,(void *)(drm+0xc8),4))owner_clients=clients;
    drm_modeset_lock_all((void *)drm);
    ret=prepare_owner(anchor,devices,drm,hdmi,qp,vop,&life_prepared,&pm_prepared);
    owner_inspect_result=ret;
    owner_link_step=link.diagnostic;owner_native_edid_present=!!link.baseline.edid_blob;
    owner_bus_format=link.baseline.bus_format;owner_pixel_clock=link.baseline.pixel_clock;
    owner_tmds_clock=link.baseline.tmds_clock;
    owner_phy_clock=link.baseline.phy_clock;
    if(owner_phase>=7)owner_native_clean=rkqms_native_link_verify_clean(&link);
    /* A native clone/check is permitted only for the specifically observed
     * clock mismatch. It is destroyed without commit, publication or writes.
     * This does not authorize recovery of an unqualified stream. */
    if(owner_phase==7 && owner_link_step==15 && !owner_native_clean){
        owner_factory_check=rkqms_native_recovery_prepare_locked((void *)drm,registers.crtc,&check);
        if(check)rkqms_native_state_put(check);
    }
    if(pm_prepared) rkqms_native_pm_discard_unpublished(&pm);
    if(life_prepared) rkqms_native_lifecycle_discard_unpublished(&life);
    drm_modeset_unlock_all((void *)drm);
    mutex_unlock((void *)(drm+0xd0));
    pr_info("RK_QMS_OWNER_CHECK: result=%d phase=%d active=%d mode=%dx%d clock=%d total=%d sync=%d; no QMS writes, publication, endpoint or BOOT proof\n",
        ret,owner_phase,owner_active,owner_width,owner_height,owner_clock,owner_vtotal,owner_sync_attempted);
    /* Keep only diagnostic parameters until normal unload. A failed board
     * check is reported, never promoted into a qualified functional owner. */
    return 0;
}
#endif

int rkqms_owner_start(unsigned long anchor,struct device **devices,unsigned long drm,
                      unsigned long hdmi,unsigned long qp,unsigned long vop)
{
    struct rkqms_session initial;
    struct rkqms_controller_ops ops={.context=&binding,.frame=rkqms_binding_frame,
        .recover=rkqms_binding_recover,.refresh=refresh};
    unsigned long head,next,prev,word;
    u32 opens;unsigned int i;int ret=-ESTALE;
    bool drm_ref=false,life_prepared=false,pm_prepared=false,started=false,endpoint=false;
    owner_phase=1;
    if(!cold_bind) return -EPERM;
    /* The factory DRM file-list mutex is +d0; head +f0, open count +c8.
     * All accesses are pinned to full original open/release/dev-init code.
     * A normal Android session is rejected even if a caller supplies cold_bind.
     */
    mutex_lock((void *)(drm+0xd0));head=drm+0xf0;
    if(copy_from_kernel_nofault(&opens,(void *)(drm+0xc8),4)) goto out;
    owner_clients=opens;
    if(opens ||
        !rkqms_reg_ptr(head,&next) || next!=head ||
        !rkqms_reg_ptr(head+8,&prev) || prev!=head) {ret=-EBUSY;goto out;}
    owner_phase=2;
    boot_verified=true;
    drm_dev_get((void *)drm);drm_ref=true;
    drm_modeset_lock_all((void *)drm);
    ret=prepare_owner(anchor,devices,drm,hdmi,qp,vop,&life_prepared,&pm_prepared);
    if(ret) goto unlock;
    owner_phase=13;
    ret=new_session(&initial,1,life.state.epoch);
unlock:
    drm_modeset_unlock_all((void *)drm);
    if(ret) goto out;
    /* Actual native OFF observation, not injected identity or a fake vblank. */
    owner_phase=14;
    ret=rkqms_binding_frame(&binding,&initial);
    if(ret || !initial.have_observation || !initial.status.observed_ns) {if(!ret)ret=-EIO;goto out;}
    owner_phase=15;
    ret=rkqms_controller_start(&controller,&initial,&ops);if(ret) goto out;
    started=true;WRITE_ONCE(controller.command_ready,false);
    owner_phase=16;
    ret=rkqms_endpoint_register(&controller);if(ret) goto out;endpoint=true;
    owner_phase=17;
    mutex_lock((void *)binding.commit_mutex);
    mutex_lock((void *)(qp+0xb48));
    if(!rkqms_reg_ptr(qp+0x7b8,&word) || word!=life.original_table ||
        !rkqms_reg_ptr(pm.driver+0x78,&word) || word!=pm.original_table) {
        ret=-ESTALE;mutex_unlock((void *)(qp+0xb48));mutex_unlock((void *)binding.commit_mutex);goto out;
    }
    /* No fallible initialization after this pin/publication point. Device
     * references from the verified probe and DRM reference persist to reboot.
     */
    __module_get(THIS_MODULE);
    for(i=0;i<4;++i) retained[i]=devices[i];
    smp_store_release((unsigned long *)(qp+0x7b8),(unsigned long)life.table);
    smp_store_release((unsigned long *)(pm.driver+0x78),(unsigned long)pm.table);
    published=true;owner_online=1;owner_phase=18;
    mutex_unlock((void *)(qp+0xb48));mutex_unlock((void *)binding.commit_mutex);
    WRITE_ONCE(controller.command_ready,true);
    pr_info("RK_QMS_OWNER: cold-bound before native DRM clients; real endpoint online; no rate switch yet; reboot-only rollback\n");
    ret=0;
out:
    if(ret) {
        if(endpoint) rkqms_endpoint_unregister();
        if(started) rkqms_controller_stop(&controller);
        if(pm_prepared) rkqms_native_pm_discard_unpublished(&pm);
        if(life_prepared) rkqms_native_lifecycle_discard_unpublished(&life);
        if(drm_ref) drm_dev_put((void *)drm);
        owner_error=ret;boot_verified=false;
        pr_info("RK_QMS_OWNER: rejected phase=%d error=%d active=%d mode=%dx%d clock=%d total=%d\n",
            owner_phase,ret,owner_active,owner_width,owner_height,owner_clock,owner_vtotal);
    }
    mutex_unlock((void *)(drm+0xd0));return ret;
}
