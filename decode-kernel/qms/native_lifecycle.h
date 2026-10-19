/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_LIFECYCLE_H
#define RKQMS_NATIVE_LIFECYCLE_H
/* Selective adaptation of 5a89a1cb's encoder-disable reset and QP re-enable
 * setup. No expansion of the original HDMI object. This header PREPARES a
 * bridge callback table; it never publishes it. The functional cold binding
 * must validate every native caller/PM route, pin the module and objects until
 * reboot, drain old atomic work before publication, and never fail module_init
 * after publishing a pointer to module code. Not a hot-unload interface.
 */
#include "native_registers.h"
struct drm_bridge;
struct drm_bridge_state;
struct drm_display_mode;
struct rkqms_native_lifecycle {
    struct rkqms_registers *registers;
    struct rkqms_lifecycle state;
    unsigned long table[25],original_table,bridge;
    unsigned int mode_calls,disable_calls,enable_calls;
    bool pm_blocked;
    void (*mode_set)(struct drm_bridge *,const struct drm_display_mode *,const struct drm_display_mode *);
    void (*disable)(struct drm_bridge *,struct drm_bridge_state *);
    void (*enable)(struct drm_bridge *,struct drm_bridge_state *);
};
static struct rkqms_native_lifecycle *rkqms_native_life;

static inline bool rkqms_native_life_matches(struct rkqms_native_lifecycle *b,struct drm_bridge *bridge)
{
    unsigned long private;
    return (unsigned long)bridge==b->bridge &&
        rkqms_reg_ptr((unsigned long)bridge+0xa0,&private) && private==b->registers->qp;
}

/* Same QP mutex as the register window. Do not acquire the controller lock:
 * a native nonblocking tail may be needed by the controller's fence wait.
 */
static inline void rkqms_native_life_event(struct rkqms_native_lifecycle *b,u32 event)
{
    struct rkqms_registers *r=b->registers;
    int ret;
    mutex_lock((void *)(r->qp+0xb48));
    rkqms_lifecycle_invalidate(&b->state,event);
    if (b->state.owns_vtem) {
        ret=rkqms_registers_disable_owned_vtem_locked(r,true);
        rkqms_lifecycle_clear_result(&b->state,ret);
    }
    mutex_unlock((void *)(r->qp+0xb48));
}
static void rkqms_native_life_mode_set(struct drm_bridge *bridge,
                  const struct drm_display_mode *mode,const struct drm_display_mode *adjusted)
{
    struct rkqms_native_lifecycle *b=rkqms_native_life;
    if (rkqms_native_life_matches(b,bridge)) {
        ++b->mode_calls;rkqms_native_life_event(b,RKQMS_LIFE_MODE);
    }
    b->mode_set(bridge,mode,adjusted);
}
static void rkqms_native_life_disable(struct drm_bridge *bridge,struct drm_bridge_state *old)
{
    struct rkqms_native_lifecycle *b=rkqms_native_life;
    if (rkqms_native_life_matches(b,bridge)) {
        ++b->disable_calls;rkqms_native_life_event(b,RKQMS_LIFE_DISABLE);
    }
    /* Owned VTEM was cleared before the factory gates dclk. If that failed,
     * the dirty flag remains; still allow ordinary native power-off.
     */
    b->disable(bridge,old);
}
static void rkqms_native_life_enable(struct drm_bridge *bridge,struct drm_bridge_state *old)
{
    struct rkqms_native_lifecycle *b=rkqms_native_life;
    bool target=rkqms_native_life_matches(b,bridge);
    if (target) {++b->enable_calls;rkqms_native_life_event(b,RKQMS_LIFE_ENABLE);}
    b->enable(bridge,old);
    /* If the earlier clear was impossible because power was off, retry only
     * after factory power-on. Clear error never counts as successful native
     * recovery, and this callback never rearms stale QMS/link/HDR identity.
     */
    if (target) rkqms_native_life_event(b,RKQMS_LIFE_ENABLE);
}

static inline int rkqms_native_lifecycle_prepare(struct rkqms_native_lifecycle *b,
                struct rkqms_registers *r,unsigned long original_table,
                unsigned long mode_set,unsigned long enable,unsigned long disable)
{
    unsigned long table,private;
    if (!b || !r || rkqms_native_life ||
        !rkqms_reg_ptr(r->qp+0x7b8,&table) || table!=original_table ||
        !rkqms_reg_ptr(r->qp+0x7c0,&private) || private!=r->qp) return -ESTALE;
    memset(b,0,sizeof(*b));b->registers=r;b->original_table=table;b->bridge=r->qp+0x720;
    if (copy_from_kernel_nofault(b->table,(void *)table,sizeof(b->table)) ||
        b->table[6]!=mode_set || b->table[10]!=enable || b->table[11]!=disable) return -ESTALE;
    b->mode_set=(void *)mode_set;b->enable=(void *)enable;b->disable=(void *)disable;
    b->table[6]=(unsigned long)rkqms_native_life_mode_set;
    b->table[10]=(unsigned long)rkqms_native_life_enable;
    b->table[11]=(unsigned long)rkqms_native_life_disable;
    rkqms_lifecycle_init(&b->state);r->lifecycle=&b->state;
    rkqms_native_life=b;
    return 0;
}
/* Only valid when the prepared table has NEVER been published. */
static inline int rkqms_native_lifecycle_discard_unpublished(struct rkqms_native_lifecycle *b)
{
    unsigned long live;
    if (rkqms_native_life!=b || !rkqms_reg_ptr(b->registers->qp+0x7b8,&live) ||
        live!=b->original_table) return -EBUSY;
    b->registers->lifecycle=NULL;rkqms_native_life=NULL;return 0;
}
#endif
