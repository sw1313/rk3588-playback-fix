/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_SESSION_H
#define RKQMS_SESSION_H
#include "transaction.h"
#include "uapi.h"

/* Sidecar storage only. Native binding serializes these calls, holds device /
 * vblank references, and supplies actual post-latch vblank observations. These
 * functions neither establish a safe programming window nor create an ioctl
 * device. Proof bits may only come from a validated native binding.
 */
struct rkqms_session {
    struct rkqms_transaction transaction;
    struct rkqms_status status;
    rq_u32 committed_sequence;
    rq_u8 awaiting_latch, have_observation, constant_reanchor;
};
int rkqms_session_init(struct rkqms_session *, const struct rkqms_mode *, rq_u8 vic,
                      const struct rkqms_caps *, const struct rkqms_io *,
                      const struct rkqms_status *identity);
int rkqms_session_submit(struct rkqms_session *, const struct rkqms_command *, int restore);
int rkqms_session_frame(struct rkqms_session *, rq_u64 generation, rq_u32 sequence, rq_u64 ns);
void rkqms_session_fault(struct rkqms_session *, int error);
void rkqms_session_disconnect(struct rkqms_session *, rq_u64 next_generation);
/* Only after native worker drain and verified scheduler/timing recovery. */
void rkqms_session_quiesced(struct rkqms_session *);
int rkqms_session_result(const struct rkqms_session *, rq_u64 generation, rq_u64 serial);
/* A constant packet/timing repeats in hardware without per-frame commits.
 * These observation APIs are internal: the binding must first verify native
 * link/epoch/power, the complete VTEM and VOP timing under native locks.
 * They never complete tickets or advance transition/fractional phases. */
int rkqms_session_constant(const struct rkqms_session *);
int rkqms_session_hold_verified(struct rkqms_session *, rq_u64, rq_u32, rq_u64);
int rkqms_session_anchor_verified(struct rkqms_session *, rq_u64, rq_u32, rq_u64);
/* A delayed worker may re-observe an unchanged integer notice, but must not
 * catch up phases, fractional timing or an awaiting ticket latch. Native
 * code must verify the complete *current* packet/timing before this call. */
int rkqms_session_repeatable(const struct rkqms_session *);
int rkqms_session_repeat_verified(struct rkqms_session *, rq_u64, rq_u32, rq_u64);
/* A late final observation is distinct from a missed programming deadline.
 * Only already committed, integer STABLE timing with M_CONST=1 qualifies.
 * Native code must recheck the full packet, latched timing/shadow, epoch and
 * link, then keep this staged result private until its native finish passes.
 * No fractional catch-up, fabricated frame, or early ticket publication. */
int rkqms_session_latch_repeatable(const struct rkqms_session *);
int rkqms_session_latch_observe_verified(struct rkqms_session *, rq_u64, rq_u32, rq_u64);
#endif
