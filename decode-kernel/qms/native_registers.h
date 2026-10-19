/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_REGISTERS_H
#define RKQMS_NATIVE_REGISTERS_H
/* Concrete original-Image I/O. Call ONLY from a tagged factory flush window
 * under all-modeset locks + the factory commit-tail mutex. Never from IRQ.
 * The factory flush, rather than a separate cfg_done write, latches timing.
 * A functional controller MUST stage its session privately and publish it only
 * after native commit + rkqms_registers_finish() succeed. This low-level file
 * does not register an endpoint, publish callbacks or certify link/HDR safety.
 */
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <uapi/drm/drm_mode.h>
#include "transaction.h"
#include "lifecycle.h"
struct drm_crtc;
struct drm_crtc_state;
struct drm_atomic_state;
extern void drm_mode_convert_to_umode(struct drm_mode_modeinfo *, const void *);
extern unsigned int drm_crtc_vblank_count(struct drm_crtc *);

/* Ordinary non-clear-on-read registers from the pinned factory dump table.
 * No HDCP keys/interrupt-clear reads; evidence never substitutes sink proof. */
struct rkqms_boundary_sample {
    u32 gcp_control,hdcp2_config,hdcp2_gpio,packet_mode,packet_rate;
    u32 dsp_control,vact,vcnt;
    int error;
};
struct rkqms_packet_stamp { u64 ns; u32 vcnt,sequence; };
struct rkqms_registers {
    struct drm_crtc *crtc;
    unsigned long qp, vop, commit_mutex;
    struct regmap *map;
    u32 *shadow;
    void __iomem *mmio;
    u32 timing_offset, saved_timing, expected_timing, scheduler_before;
    u32 control_offset,vact_offset,status_offset;
    u32 sequence;
    rq_u64 generation;
    struct rkqms_mode base;
    struct task_struct *task;
    struct drm_atomic_state *transaction;
    struct drm_crtc_state *old;
    void (*flush)(struct drm_crtc *, struct drm_crtc_state *);
    unsigned char held, flushed, vtem_before, expected_vtem;
    int error, diagnostic;
    u32 observed_timing, observed_shadow;
    u32 completion_sequence,completion_timing;
    u32 observed_scheduler;
    unsigned char capture_boundary;
    struct rkqms_boundary_sample boundary_before,boundary_after;
    struct rkqms_packet_stamp mode_write,rate_write;
    struct rkqms_lifecycle *lifecycle;
    rq_u64 expected_epoch;
};

/* Caller has already checked power and holds the QP mutex, no VOP spinlock.
 * A diagnostic read failure is recorded, never a reason to accept/fail I/O. */
static inline void rkqms_registers_sample(struct rkqms_registers *r,
                                         struct rkqms_boundary_sample *s)
{
    memset(s,0,sizeof(*s));
    s->error=regmap_read(r->map,0xaac,&s->gcp_control);
    if (!s->error)s->error=regmap_read(r->map,0x8e0,&s->hdcp2_config);
    if (!s->error)s->error=regmap_read(r->map,0x8e8,&s->hdcp2_gpio);
    if (!s->error)s->error=regmap_read(r->map,0xd08,&s->packet_mode);
    if (!s->error)s->error=regmap_read(r->map,0xd0c,&s->packet_rate);
    if (!s->error) {
        s->dsp_control=readl(r->mmio+r->control_offset);
        s->vact=readl(r->mmio+r->vact_offset);
        s->vcnt=readl(r->mmio+r->status_offset)>>16;
    }
}

static inline int rkqms_reg_ptr(unsigned long p, unsigned long *v)
{
    return p && !copy_from_kernel_nofault(v, (void *)p, sizeof(*v));
}

static inline int rkqms_registers_bind(struct rkqms_registers *r, struct drm_crtc *crtc,
                                      unsigned long qp, unsigned long vop,
                                      unsigned long expected_data, unsigned long expected_vp_regs,
                                      const struct rkqms_mode *base)
{
    unsigned long owner, data, ports, table, crtc_table, shadow, mmio, map;
    unsigned char port;
    u32 descriptor[3],control[3],vact[3],version;
    if (!r || !base || !rkqms_reg_ptr((unsigned long)crtc+0x668, &owner) || owner!=vop ||
        copy_from_kernel_nofault(&port, (char *)crtc+0x688, 1) || port>3 ||
        copy_from_kernel_nofault(&version, (void *)vop, 4) || version!=0x4017 ||
        !rkqms_reg_ptr(vop+0x2858, &data) || data!=expected_data ||
        !rkqms_reg_ptr(data+0x48, &ports) ||
        !rkqms_reg_ptr(ports+port*0x60+0x58, &table) || table!=expected_vp_regs ||
        !rkqms_reg_ptr((unsigned long)crtc+0x690, &crtc_table) || crtc_table!=table ||
        /* Native enable loads descriptor +0x9c for vertical total/pulse.
         * +0x90 / register0xc4c is HACT, NOT vertical timing.
         */
        copy_from_kernel_nofault(descriptor, (void *)(table+0x9c), sizeof(descriptor)) ||
        descriptor[0]!=0x1fff1fffU ||
        descriptor[1]!=(u32)(0xc50+port*0x100) || descriptor[2] ||
        /* Original Image standby and VACT descriptors, not newer C layout. */
        copy_from_kernel_nofault(control,(void *)(table+0x3c),sizeof(control)) ||
        control[0]!=1 || control[1]!=((31U<<17)|(0xc00+port*0x100)) || control[2] ||
        copy_from_kernel_nofault(vact,(void *)(table+0xa8),sizeof(vact)) ||
        vact[0]!=0x1fff1fffU || vact[1]!=(u32)(0xc54+port*0x100) || vact[2] ||
        !rkqms_reg_ptr(vop+0x2868, &shadow) || !shadow || (shadow&3) ||
        !rkqms_reg_ptr(vop+0x2870, &mmio) || !mmio || (mmio&3) ||
        !rkqms_reg_ptr(qp+0xbe0, &map) || !map ||
        regmap_get_val_bytes((void *)map)!=4 || regmap_get_reg_stride((void *)map)!=4)
        return -ESTALE;
    memset(r, 0, sizeof(*r));
    r->crtc=crtc;r->qp=qp;r->vop=vop;r->map=(void *)map;
    r->shadow=(void *)shadow;r->mmio=(void __iomem *)mmio;
    r->timing_offset=descriptor[1];r->base=*base;
    r->control_offset=control[1]&0x1ffffU;r->vact_offset=vact[1];
    /* RK3588/RK3568 SYS_STATUS0, read-only scan line in bits31:16.
     * Pinned VOP version0x4017 and port identity checked above. No IRQ status
     * or clear-on-read counters are read by this diagnostic. */
    r->status_offset=0x60+port*4;
    return 0;
}

static inline int rkqms_registers_begin(void *p, rq_u64 generation, rq_u32 frame)
{
    struct rkqms_registers *r=p;
    struct drm_mode_modeinfo mode;
    unsigned long owner, live;
    unsigned char dclk, disabled, active;
    u32 timing;
    int ret;
    if (!r) return -EINVAL;
    r->diagnostic=1;
    if (!r || generation!=r->generation || frame!=r->sequence || r->held ||
        current!=r->task || in_interrupt() || !r->flush ||
        !rkqms_reg_ptr(r->commit_mutex, &owner) || (owner&~7UL)!=(unsigned long)current ||
        !rkqms_reg_ptr((unsigned long)r->old+0x148, &owner) || owner!=(unsigned long)r->transaction ||
        drm_crtc_vblank_count(r->crtc)!=frame) return -ESTALE;
    r->diagnostic=2;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    r->diagnostic=3;
    if (r->lifecycle && rkqms_lifecycle_check(r->lifecycle,r->expected_epoch)) {
        ret=-ESTALE;goto unlock;
    }
    if (copy_from_kernel_nofault(&dclk, (void *)(r->qp+0xb42), 1) || !dclk ||
        copy_from_kernel_nofault(&disabled, (void *)(r->qp+0xb74), 1) || disabled ||
        !rkqms_reg_ptr((unsigned long)r->crtc+0x5c0, &live) ||
        copy_from_kernel_nofault(&active, (void *)(live+9), 1) || !active) {
        ret=-ESTALE;goto unlock;
    }
    r->diagnostic=4;
    memset(&mode, 0, sizeof(mode));
    drm_mode_convert_to_umode(&mode, (void *)(live+0x18));
    if (mode.clock!=r->base.clock_khz || mode.flags!=r->base.flags ||
        mode.hdisplay!=r->base.width || mode.vdisplay!=r->base.height ||
        mode.htotal!=r->base.htotal || mode.vtotal!=r->base.vtotal ||
        mode.vsync_start!=r->base.vsync_start || mode.vsync_end!=r->base.vsync_end) {
        ret=-ESTALE;goto unlock;
    }
    r->diagnostic=5;
    ret=regmap_read(r->map, 0xaa8, &r->scheduler_before);
    if (ret) goto unlock;
    if (r->capture_boundary)rkqms_registers_sample(r,&r->boundary_before);
    if (!!(r->scheduler_before&BIT(9))!=r->vtem_before) {ret=-ESTALE;goto unlock;}
    r->diagnostic=6;
    spin_lock((void *)(r->vop+0x28b0));
    timing=READ_ONCE(r->shadow[r->timing_offset/4]);
    r->observed_shadow=timing;r->observed_timing=readl(r->mmio+r->timing_offset);
    /* Keep VS_END and reserved bits exactly. This profile deliberately
     * refuses immediate-mode bit15 instead of clearing a guessed field.
     */
    if ((timing&0xffff)!=(u32)(r->base.vsync_end-r->base.vsync_start) ||
        r->observed_timing!=timing ||
        (!r->expected_timing && ((timing>>16)&0x1fff)!=r->base.vtotal) ||
        (r->expected_timing && r->expected_timing!=timing)) {
        spin_unlock((void *)(r->vop+0x28b0));ret=-ESTALE;goto unlock;
    }
    r->saved_timing=timing;r->expected_timing=timing;r->expected_vtem=r->vtem_before;r->held=1;
    r->diagnostic=7;
    return 0;
unlock:
    mutex_unlock((void *)(r->qp+0xb48));
    return ret;
}

static inline int rkqms_registers_packet(void *p, rq_u32 offset, rq_u32 value)
{
    struct rkqms_registers *r=p;
    int ret;
    if (!r->held || current!=r->task || offset<0xd00 || offset>0xd1c || (offset&3)) return -EINVAL;
    ret=regmap_write(r->map, offset, value);
    if (!ret && r->capture_boundary && (offset==0xd08 || offset==0xd0c)) {
        struct rkqms_packet_stamp *s=offset==0xd08?&r->mode_write:&r->rate_write;
        s->ns=ktime_get_ns();s->vcnt=readl(r->mmio+r->status_offset)>>16;
        s->sequence=drm_crtc_vblank_count(r->crtc);
    }
    return ret;
}
static inline int rkqms_registers_enable(void *p, rq_u32 mask, rq_u32 value)
{
    struct rkqms_registers *r=p;
    int ret;
    if (!r->held || current!=r->task || mask!=BIT(9) || (value&~mask)) return -EINVAL;
    if (value && r->lifecycle) rkqms_lifecycle_claim_vtem(r->lifecycle);
    ret=regmap_update_bits(r->map, 0xaa8, mask, value);
    if (!ret) r->expected_vtem=!!value;
    return ret;
}
static inline int rkqms_registers_timing(void *p, rq_u16 total, rq_u16 vfp)
{
    struct rkqms_registers *r=p;
    u32 value;
    if (!r->held || current!=r->task || total<r->base.vtotal || total>0x1fff ||
        vfp!=(u32)(r->base.vsync_start-r->base.height+total-r->base.vtotal)) return -EINVAL;
    value=(r->saved_timing&0xe000ffffU)|((u32)total<<16);
    WRITE_ONCE(r->shadow[r->timing_offset/4], value);
    writel(value, r->mmio+r->timing_offset);
    r->expected_timing=value;
    /* No independent cfg_done. The same native flush handles all planes. */
    return 0;
}
static inline void rkqms_registers_end(void *p)
{
    struct rkqms_registers *r=p;
    if (r->held) {
        r->held=0;
        spin_unlock((void *)(r->vop+0x28b0));
        mutex_unlock((void *)(r->qp+0xb48));
    }
    if (!r->flushed) {r->flushed=1;r->flush(r->crtc,r->old);}
}
static inline int rkqms_registers_finish(struct rkqms_registers *r)
{
    u32 enable;
    unsigned char powered,disabled,active;
    unsigned long live;
    int ret;
    if (!r) return -EIO;
    r->completion_sequence=0;r->completion_timing=0;r->diagnostic=80;
    if (!r->flushed || r->held || current!=r->task) return -EIO;
    r->diagnostic=81;r->completion_sequence=drm_crtc_vblank_count(r->crtc);
    if ((u32)(r->completion_sequence-r->sequence)!=1) return -EIO;
    /* Native nonblocking tails can outlive the userspace modeset locks.
     * Check power under the QP mutex before touching VOP MMIO at completion,
     * just as begin() does. Never read an already powered-down timing block.
     */
    r->diagnostic=82;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    r->diagnostic=83;
    if ((r->lifecycle && rkqms_lifecycle_check(r->lifecycle,r->expected_epoch)) ||
        copy_from_kernel_nofault(&powered,(void *)(r->qp+0xb42),1) || !powered ||
        copy_from_kernel_nofault(&disabled,(void *)(r->qp+0xb74),1) || disabled ||
        !rkqms_reg_ptr((unsigned long)r->crtc+0x5c0,&live) ||
        copy_from_kernel_nofault(&active,(void *)(live+9),1) || !active) {
        ret=-ESTALE;goto out;
    }
    r->diagnostic=84;r->completion_timing=readl(r->mmio+r->timing_offset);
    if (r->completion_timing!=r->expected_timing) {ret=-EIO;goto out;}
    r->diagnostic=85;
    ret=regmap_read(r->map, 0xaa8, &enable);
    if (ret) goto out;
    r->observed_scheduler=enable;
    if (r->capture_boundary)rkqms_registers_sample(r,&r->boundary_after);
    /* Metadata/audio packet enables must survive every transaction. */
    ret=((enable^r->scheduler_before)&~BIT(9)) ||
        !!(enable&BIT(9))!=r->expected_vtem ? -ESTALE : 0;
    if (!ret && r->lifecycle && !r->expected_vtem)
        rkqms_lifecycle_clear_result(r->lifecycle,0);
    if (!ret)r->diagnostic=0;
out:
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}

/* Read-only qualification of autonomous CONSTANT output, under all-modeset
 * locks and the factory commit mutex. Does not replace any native commit,
 * phase/latch check, fractional update or fence wait. Both a passive observer
 * and the first transition window after hold use exactly this qualification. */
static inline int rkqms_registers_repeat(struct rkqms_registers *r,const struct rkqms_state *s)
{
    const struct rkqms_timing *t;
    unsigned long owner,live;
    unsigned char powered,disabled,active;
    u32 enable,value,packet[8]={0},timing,shadow;
    int ret,i;
    if (!r || !s || r->held || in_interrupt()) return -EINVAL;
    r->diagnostic=100;t=rkqms_timing(s->vic,s->current_tfr);
    if (!t || t->bits || !s->vtem_enabled || s->packet_const>1 ||
        !s->packet_tfr || s->packet_tfr>13 || s->current_vtotal!=t->vtotal ||
        !r->expected_vtem || ((r->expected_timing>>16)&0x1fff)!=t->vtotal ||
        !rkqms_reg_ptr(r->commit_mutex,&owner) ||
        (owner&~7UL)!=(unsigned long)current) return -ESTALE;
    r->diagnostic=101;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    r->diagnostic=102;
    if (!r->lifecycle || !r->lifecycle->owns_vtem ||
        rkqms_lifecycle_check(r->lifecycle,r->expected_epoch) ||
        copy_from_kernel_nofault(&powered,(void *)(r->qp+0xb42),1) || !powered ||
        copy_from_kernel_nofault(&disabled,(void *)(r->qp+0xb74),1) || disabled ||
        !rkqms_reg_ptr((unsigned long)r->crtc+0x5c0,&live) ||
        copy_from_kernel_nofault(&active,(void *)(live+9),1) || !active) {
        ret=-ESTALE;goto out;
    }
    r->diagnostic=103;
    ret=regmap_read(r->map,0xaa8,&enable);if(ret)goto out;
    r->observed_scheduler=enable;
    /* Only VTEM belongs to QMS. Audio/HDR packet enables may be changed by
     * their native owners between our transactions. An old before-snapshot
     * is not a lifetime invariant. begin/finish still verify preservation
     * across each actual QMS commit; this path is strictly read-only. */
    if (!(enable&BIT(9))) {ret=-ESTALE;goto out;}
    packet[0]=0xc0U<<8;packet[1]=RKQMS_VTEM_DATASET_FLAGS;packet[2]=((s->packet_const?6U:4U)<<24)|(4U<<16);
    packet[3]=((s->base_tfr==13?120U:60U)<<16)|((u32)s->packet_tfr<<11)|
        (s->base.vsync_start-s->base.height);
    r->diagnostic=104;
    for(i=0;i<8;++i) {
        ret=regmap_read(r->map,0xd00+i*4,&value);if(ret)goto out;
        if(value!=packet[i]) {ret=-ESTALE;goto out;}
    }
    r->diagnostic=105;
    /* commit_mutex excludes factory tails; QP mutex pins power/epoch. The
     * atomic 32-bit read also checks the native shadow, with no register write. */
    shadow=READ_ONCE(r->shadow[r->timing_offset/4]);timing=readl(r->mmio+r->timing_offset);
    ret=shadow!=r->expected_timing || timing!=r->expected_timing ? -ESTALE : 0;
    if(!ret)r->diagnostic=0;
out:
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}

static inline int rkqms_registers_hold(struct rkqms_registers *r,const struct rkqms_state *s)
{
    if (!r || !s) return -EINVAL;
    r->diagnostic=100;
    if (s->packet_const!=1 || s->packet_tfr!=s->current_tfr) return -ESTALE;
    return rkqms_registers_repeat(r,s);
}

/* Process context, native all-modeset locks held, QMS worker drained. Do not
 * assume a factory modeset clears VTEM: this firmware never implemented it.
 * An unowned enabled bit is a binding conflict, not permission to clear it.
 */
/* QP mutex already held. Do not call with r->held / VOP spinlock held. */
static inline int rkqms_registers_disable_owned_vtem_locked(struct rkqms_registers *r,bool owned)
{
    unsigned char powered,disabled;
    u32 before,after;
    int ret;
    if (!r || r->held || in_interrupt()) return -EINVAL;
    if (copy_from_kernel_nofault(&powered,(void *)(r->qp+0xb42),1) || !powered ||
        copy_from_kernel_nofault(&disabled,(void *)(r->qp+0xb74),1) || disabled) {
        ret=-ESTALE;goto out;
    }
    ret=regmap_read(r->map,0xaa8,&before);
    if (ret) goto out;
    if ((before&BIT(9)) && !owned) {ret=-EBUSY;goto out;}
    if (before&BIT(9)) {
        ret=regmap_update_bits(r->map,0xaa8,BIT(9),0);
        if (ret) goto out;
    }
    ret=regmap_read(r->map,0xaa8,&after);
    if (!ret && after!=(before&~BIT(9))) ret=-EIO;
out:
    return ret;
}
static inline int rkqms_registers_disable_owned_vtem(struct rkqms_registers *r,bool owned)
{
    int ret;
    if (!r || r->held || in_interrupt()) return -EINVAL;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    ret=rkqms_registers_disable_owned_vtem_locked(r,owned);
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}

/* Only the register/power portion of recovery. Caller must separately verify
 * actual native mode/connector/link/HDR identity and old worker retirement.
 */
static inline int rkqms_registers_verify_base(struct rkqms_registers *r)
{
    unsigned char powered,disabled;
    u32 scheduler,timing,shadow;
    int ret;
    if (!r || r->held || in_interrupt()) return -EINVAL;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    if (copy_from_kernel_nofault(&powered,(void *)(r->qp+0xb42),1) || !powered ||
        copy_from_kernel_nofault(&disabled,(void *)(r->qp+0xb74),1) || disabled) {
        ret=-ESTALE;goto out;
    }
    ret=regmap_read(r->map,0xaa8,&scheduler);
    if (ret) goto out;
    spin_lock((void *)(r->vop+0x28b0));
    timing=readl(r->mmio+r->timing_offset);
    shadow=READ_ONCE(r->shadow[r->timing_offset/4]);
    spin_unlock((void *)(r->vop+0x28b0));
    if ((scheduler&BIT(9)) || shadow!=timing ||
        ((timing>>16)&0x1fff)!=r->base.vtotal ||
        (timing&0xffff)!=(u32)(r->base.vsync_end-r->base.vsync_start)) ret=-ESTALE;
out:
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}
#endif
