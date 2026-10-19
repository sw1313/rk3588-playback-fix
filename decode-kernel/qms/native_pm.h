/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_PM_H
#define RKQMS_NATIVE_PM_H
/* Prepare the exact factory HDMI PM table with six system-sleep wrappers.
 * The native table has NO runtime/late/noirq callbacks; any such addition is
 * an incompatible profile, not an unguarded power route. Preparation never
 * publishes pointers. Cold binding/pinning constraints in native_lifecycle.h
 * also apply to this driver-shared table; other HDMI ports pass through.
 */
#include "native_lifecycle.h"
struct device;
struct rkqms_native_pm {
    struct rkqms_native_lifecycle *life;
    unsigned long device,driver,original_table,table[23];
    int (*suspend)(struct device *);
    int (*resume)(struct device *);
    int (*quiesce)(void *);
    void *context;
};
static struct rkqms_native_pm *rkqms_pm_binding;
static inline void rkqms_pm_record_error(struct rkqms_native_pm *p,int error)
{
    if (error) {
        mutex_lock((void *)(p->life->registers->qp+0xb48));
        rkqms_lifecycle_clear_result(&p->life->state,error);
        mutex_unlock((void *)(p->life->registers->qp+0xb48));
    }
}
static int rkqms_pm_suspend(struct device *device)
{
    struct rkqms_native_pm *p=rkqms_pm_binding;
    int ret;
    if ((unsigned long)device==p->device) {
        WRITE_ONCE(p->life->pm_blocked,true);
        /* This call must be bounded and independent of a native fence-held
         * controller mutex. Failure aborts this suspend BEFORE factory power
         * gating. Never ignore failure to clear this binding's VTEM.
         */
        ret=p->quiesce(p->context);
        if (ret) return ret;
        rkqms_native_life_event(p->life,RKQMS_LIFE_SUSPEND);
        if (p->life->state.owns_vtem || p->life->state.dirty || p->life->state.error) return -EBUSY;
    }
    ret=p->suspend(device);
    if ((unsigned long)device==p->device) rkqms_pm_record_error(p,ret);
    return ret;
}
static int rkqms_pm_resume(struct device *device)
{
    struct rkqms_native_pm *p=rkqms_pm_binding;
    int ret=p->resume(device);
    if ((unsigned long)device==p->device) {
        rkqms_native_life_event(p->life,RKQMS_LIFE_SUSPEND);
        rkqms_pm_record_error(p,ret);
        if(!ret && !p->life->state.owns_vtem && !p->life->state.dirty && !p->life->state.error)
            WRITE_ONCE(p->life->pm_blocked,false);
    }
    return ret;
}
static inline int rkqms_native_pm_prepare(struct rkqms_native_pm *p,
          struct rkqms_native_lifecycle *life,struct device *device,unsigned long expected_table,
          unsigned long suspend,unsigned long resume,int (*quiesce)(void *),void *context)
{
    unsigned long driver,table;
    unsigned int i;
    if (!p || !life || !quiesce || rkqms_pm_binding ||
        !rkqms_reg_ptr((unsigned long)device+0x68,&driver) || !driver ||
        !rkqms_reg_ptr(driver+0x78,&table) || table!=expected_table) return -ESTALE;
    memset(p,0,sizeof(*p));p->device=(unsigned long)device;p->driver=driver;
    p->life=life;p->original_table=table;p->quiesce=quiesce;p->context=context;
    if (copy_from_kernel_nofault(p->table,(void *)table,sizeof(p->table))) return -EFAULT;
    for (i=0;i<23;++i) {
        unsigned long expected=i>=2 && i<=7 ? (i&1 ? resume : suspend) : 0;
        if (p->table[i]!=expected) return -ESTALE;
    }
    p->suspend=(void *)suspend;p->resume=(void *)resume;
    for (i=2;i<=7;++i) p->table[i]=(unsigned long)(i&1 ? rkqms_pm_resume : rkqms_pm_suspend);
    rkqms_pm_binding=p;return 0;
}
static inline int rkqms_native_pm_discard_unpublished(struct rkqms_native_pm *p)
{
    unsigned long table;
    if (rkqms_pm_binding!=p || !rkqms_reg_ptr(p->driver+0x78,&table) ||
        table!=p->original_table) return -EBUSY;
    rkqms_pm_binding=NULL;return 0;
}
#endif
