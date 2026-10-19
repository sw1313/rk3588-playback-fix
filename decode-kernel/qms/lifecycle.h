/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_LIFECYCLE_H
#define RKQMS_LIFECYCLE_H
#include "core.h"
/* Sidecar equivalent of upstream encoder_atomic_disable's QMS reset, extended
 * to fail closed on native mode/power events. Serialize writers with the QP
 * mutex. Register windows recheck epoch/blocked under that SAME mutex. These
 * flags alone are not a safe hardware window, hotplug detector or boot proof.
 */
#define RKQMS_LIFE_MODE 1U
#define RKQMS_LIFE_DISABLE 2U
#define RKQMS_LIFE_ENABLE 4U
#define RKQMS_LIFE_SUSPEND 8U
#define RKQMS_LIFE_LINK_LOST 16U
struct rkqms_lifecycle {
    rq_u64 epoch;
    rq_u32 blocked, reasons;
    rq_u8 owns_vtem, dirty;
    int error;
};
void rkqms_lifecycle_init(struct rkqms_lifecycle *);
int rkqms_lifecycle_arm(struct rkqms_lifecycle *,rq_u64 verified_epoch);
int rkqms_lifecycle_check(const struct rkqms_lifecycle *,rq_u64 expected_epoch);
void rkqms_lifecycle_invalidate(struct rkqms_lifecycle *,rq_u32 reason);
void rkqms_lifecycle_claim_vtem(struct rkqms_lifecycle *);
void rkqms_lifecycle_clear_result(struct rkqms_lifecycle *,int error);
#endif
