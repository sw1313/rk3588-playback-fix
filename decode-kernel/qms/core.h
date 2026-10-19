/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_CORE_H
#define RKQMS_CORE_H
#ifdef __KERNEL__
#include <linux/types.h>
typedef u8 rq_u8;
typedef u16 rq_u16;
typedef u32 rq_u32;
typedef u64 rq_u64;
#else
#include <stdint.h>
typedef uint8_t rq_u8;
typedef uint16_t rq_u16;
typedef uint32_t rq_u32;
typedef uint64_t rq_u64;
#endif

/* Private state only: never append these fields to factory DRM allocations.
 * Caller serializes access and verifies device/link/locks before ANY HW action.
 * plan() has no side effects. complete() publishes only a successful action.
 */
enum rkqms_phase {
    RKQMS_OFF, RKQMS_BASE_CONST, RKQMS_BASE_VARIABLE, RKQMS_TARGET_NOTICE,
    RKQMS_APPLY, RKQMS_CONFIRM, RKQMS_STABLE,
    RKQMS_RESTORE_NOTICE, RKQMS_RESTORE_APPLY, RKQMS_RESTORE_SETTLE,
    RKQMS_RESTORE_CONST, RKQMS_RESTORE_OFF, RKQMS_FAULT
};
enum { RKQMS_EINVAL = -22, RKQMS_EBUSY = -16, RKQMS_ESTALE = -116,
       RKQMS_ENOTSUP = -95, RKQMS_EIO = -5, RKQMS_EAGAIN = -11 };
/* Vendor packet retained. Both the matrix-path trial and the direct-sink
 * END=0 re-test failed physical acceptance; the latter was withdrawn too.
 * Data-set END is distinct from packet FIRST/LAST. */
#define RKQMS_VTEM_DATASET_FLAGS 0xc6U
struct rkqms_timing {
    rq_u8 vic, tfr;
    rq_u16 vtotal;
    rq_u8 bits, fraction[16];
};
struct rkqms_mode {
    rq_u32 clock_khz, flags;
    rq_u16 width, height, htotal, vtotal, vsync_start, vsync_end;
};
struct rkqms_caps {
    rq_u32 tfr_mask; /* Sink + link + exact backend proof, not EDID QMS bit alone. */
    rq_u8 qms, unchanged_link;
};
struct rkqms_state {
    rq_u64 generation, revision, transaction;
    struct rkqms_mode base;
    struct rkqms_caps caps;
    rq_u32 last_sequence, fraction_frame;
    rq_u16 current_vtotal;
    rq_u8 vic, base_tfr, current_tfr, requested_tfr;
    rq_u8 phase, phase_frames, sequence_valid, vtem_enabled, packet_tfr, packet_const;
    int error;
};
struct rkqms_action {
    rq_u64 generation, revision;
    rq_u32 sequence;
    rq_u16 vtotal, vfp;
    rq_u8 write_timing, write_vtem, vtem_enabled, m_const, next_tfr;
    rq_u32 vtem[8];
    struct rkqms_state next;
};
const struct rkqms_timing *rkqms_timing(rq_u8 vic, rq_u8 tfr);
rq_u16 rkqms_vtotal(const struct rkqms_timing *, rq_u32 frame);
int rkqms_rate(rq_u8 tfr, rq_u32 *numerator, rq_u32 *denominator);
int rkqms_init(struct rkqms_state *, const struct rkqms_mode *, rq_u8 vic,
               const struct rkqms_caps *, rq_u64 generation);
int rkqms_request(struct rkqms_state *, rq_u8 tfr, rq_u64 generation, rq_u64 transaction);
int rkqms_plan(const struct rkqms_state *, rq_u64 generation, rq_u32 sequence,
               struct rkqms_action *);
int rkqms_complete(struct rkqms_state *, const struct rkqms_action *, int hardware_error);
void rkqms_invalidate(struct rkqms_state *, rq_u64 new_generation);
#endif
