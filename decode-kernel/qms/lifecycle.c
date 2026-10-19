// SPDX-License-Identifier: GPL-2.0-only
#include "lifecycle.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
void rkqms_lifecycle_init(struct rkqms_lifecycle *s)
{
    memset(s,0,sizeof(*s));s->epoch=1;s->blocked=1;
}
int rkqms_lifecycle_arm(struct rkqms_lifecycle *s,rq_u64 verified_epoch)
{
    if (!s || !verified_epoch || verified_epoch!=s->epoch) return RKQMS_ESTALE;
    if (s->dirty || s->owns_vtem || s->error) return RKQMS_EBUSY;
    s->blocked=0;s->reasons=0;return 0;
}
int rkqms_lifecycle_check(const struct rkqms_lifecycle *s,rq_u64 expected_epoch)
{
    if (!s || !expected_epoch || expected_epoch!=s->epoch || s->blocked) return RKQMS_ESTALE;
    return s->error ? s->error : 0;
}
void rkqms_lifecycle_invalidate(struct rkqms_lifecycle *s,rq_u32 reason)
{
    s->blocked=1;s->reasons|=reason;
    /* Epoch zero is permanently unusable after wrap. Never reuse an old
     * identity even if several native callbacks refer to one transaction.
     */
    if (s->epoch) ++s->epoch;
}
void rkqms_lifecycle_claim_vtem(struct rkqms_lifecycle *s)
{
    /* Call BEFORE trying to set TX_EN: a failed regmap operation may have
     * reached hardware and still requires owned-bit cleanup.
     */
    s->owns_vtem=1;s->dirty=1;
}
void rkqms_lifecycle_clear_result(struct rkqms_lifecycle *s,int error)
{
    if (error) {s->error=error<0?error:RKQMS_EIO;s->dirty=1;return;}
    s->owns_vtem=0;s->dirty=0;s->error=0;
    /* A clear bit does not authorize resuming QMS after a native transition.
     * Mode/link/HDR/power must be revalidated and arm() called explicitly.
     */
}
