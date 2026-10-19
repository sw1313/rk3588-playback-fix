// SPDX-License-Identifier: GPL-2.0-only
/* Multi-frame QMS orchestration, selectively derived from Rockchip
 * 5a89a1cb8b9c3cba5666151e838d2a3d760fe0ea. No work or waits in IRQ context.
 * Upstream's positive wait_event_timeout result is NOT an error here: the
 * backend explicitly completes each register transaction and each real frame.
 */
#include "core.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
#include "timings.h"
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

const struct rkqms_timing *rkqms_timing(rq_u8 vic, rq_u8 tfr)
{
    unsigned int i;
    if (vic == 102)
        vic = 97;
    for (i = 0; i < COUNT(rkqms_timings); ++i)
        if (rkqms_timings[i].vic == vic && rkqms_timings[i].tfr == tfr)
            return &rkqms_timings[i];
    return 0;
}

rq_u16 rkqms_vtotal(const struct rkqms_timing *timing, rq_u32 frame)
{
    rq_u32 bit;
    if (!timing || timing->bits > 128)
        return 0;
    if (!timing->bits)
        return timing->vtotal;
    bit = frame % timing->bits;
    return timing->vtotal + ((timing->fraction[bit / 8] >> (7 - bit % 8)) & 1);
}

int rkqms_rate(rq_u8 tfr, rq_u32 *num, rq_u32 *den)
{
    static const rq_u32 numerators[] = {0, 24000, 24, 25, 30000, 30, 48000, 48,
                                        50, 60000, 60, 100, 120000, 120};
    if (!num || !den || !tfr || tfr >= COUNT(numerators))
        return RKQMS_EINVAL;
    *num = numerators[tfr];
    *den = tfr == 1 || tfr == 4 || tfr == 6 || tfr == 9 || tfr == 12 ? 1001 : 1;
    return 0;
}

int rkqms_init(struct rkqms_state *s, const struct rkqms_mode *mode, rq_u8 vic,
               const struct rkqms_caps *caps, rq_u64 generation)
{
    rq_u16 width, height, htotal, vtotal, vs, ve;
    rq_u32 clock;
    rq_u8 base_tfr = 10;
    if (!s || !mode || !caps || !generation || !caps->qms || !caps->unchanged_link)
        return RKQMS_ENOTSUP;
    switch (vic) {
    case 4: case 47:
        width = 1280; height = 720; htotal = 1650; vtotal = 750; vs = 725; ve = 730;
        clock = vic == 4 ? 74250 : 148500;
        if (vic == 47) base_tfr = 13;
        break;
    case 16: case 63:
        width = 1920; height = 1080; htotal = 2200; vtotal = 1125; vs = 1084; ve = 1089;
        clock = vic == 16 ? 148500 : 297000;
        if (vic == 63) base_tfr = 13;
        break;
    case 97: case 102:
        width = vic == 97 ? 3840 : 4096; height = 2160; htotal = 4400;
        vtotal = 2250; vs = 2168; ve = 2178; clock = 594000;
        break;
    default: return RKQMS_ENOTSUP;
    }
    /* Positive H/V sync only. Interlace/doublescan/3D and fractional BRR are
     * outside these upstream timing tables; do not silently approximate them.
     */
    if (mode->flags != 5 || mode->width != width || mode->height != height ||
        mode->htotal != htotal || mode->vtotal != vtotal || mode->clock_khz != clock ||
        mode->vsync_start != vs || mode->vsync_end != ve ||
        !(caps->tfr_mask & (1U << base_tfr)))
        return RKQMS_ENOTSUP;
    memset(s, 0, sizeof(*s));
    s->base = *mode;
    s->caps = *caps;
    s->generation = generation;
    s->revision = 1;
    s->vic = vic;
    s->base_tfr = s->current_tfr = base_tfr;
    s->current_vtotal = vtotal;
    return 0;
}

int rkqms_request(struct rkqms_state *s, rq_u8 tfr, rq_u64 generation, rq_u64 transaction)
{
    if (!s || generation != s->generation || !transaction)
        return RKQMS_ESTALE;
    if (s->phase == RKQMS_FAULT)
        return s->error ? s->error : RKQMS_EIO;
    if (s->phase != RKQMS_OFF && s->phase != RKQMS_STABLE)
        return RKQMS_EBUSY;
    if (tfr && (!(s->caps.tfr_mask & (1U << (tfr < 14 ? tfr : 0))) ||
                !rkqms_timing(s->vic, tfr)))
        return RKQMS_ENOTSUP;
    s->transaction = transaction;
    s->requested_tfr = tfr;
    /* OFF does not maintain a VTEM stream or a fractional cadence. The first
     * real frame of a new activation may follow an arbitrarily long idle.
     * Once enabled, every subsequent frame still requires exactly +1. */
    if (s->phase == RKQMS_OFF) s->sequence_valid = 0;
    if ((!tfr && s->phase == RKQMS_OFF) || (tfr && s->phase == RKQMS_STABLE && tfr == s->current_tfr)) {
        ++s->revision;
        return 0;
    }
    if (!tfr)
        s->phase = RKQMS_RESTORE_NOTICE;
    else if (s->phase == RKQMS_OFF)
        s->phase = RKQMS_BASE_CONST;
    else
        s->phase = RKQMS_BASE_VARIABLE;
    s->phase_frames = 0;
    ++s->revision;
    return 0;
}

static void packet(const struct rkqms_state *s, struct rkqms_action *a,
                   rq_u8 tfr, rq_u8 constant)
{
    rq_u32 base_rate = s->base_tfr == 13 ? 120 : 60;
    a->vtem_enabled = !!tfr;
    a->next_tfr = tfr;
    a->m_const = constant;
    a->write_vtem = s->vtem_enabled != a->vtem_enabled ||
                    (tfr && (s->packet_tfr != tfr || s->packet_const != constant));
    if (!tfr)
        return;
    a->vtem[0] = 0xc0U << 8;
    /* Vendor packet retained after the independent direct-sink END=0
     * re-test also failed. Keep other signal changes separate. */
    a->vtem[1] = RKQMS_VTEM_DATASET_FLAGS;
    a->vtem[2] = ((constant ? 6U : 4U) << 24) | (4U << 16);
    a->vtem[3] = (base_rate << 16) | ((rq_u32)tfr << 11) |
                 (s->base.vsync_start - s->base.height);
}

static int timing(const struct rkqms_state *s, struct rkqms_action *a, rq_u8 tfr, rq_u32 frame)
{
    rq_u16 total = rkqms_vtotal(rkqms_timing(s->vic, tfr), frame);
    if (total < s->base.vtotal)
        return RKQMS_ENOTSUP;
    a->vtotal = total;
    a->vfp = s->base.vsync_start - s->base.height + total - s->base.vtotal;
    a->write_timing = total != s->current_vtotal;
    a->next.current_vtotal = total;
    return 0;
}

int rkqms_plan(const struct rkqms_state *s, rq_u64 generation, rq_u32 sequence,
               struct rkqms_action *a)
{
    int ret;
    rq_u8 phase;
    if (!s || !a || generation != s->generation)
        return RKQMS_ESTALE;
    if (s->phase == RKQMS_FAULT)
        return s->error ? s->error : RKQMS_EIO;
    if (s->phase == RKQMS_OFF)
        return RKQMS_EAGAIN;
    /* No synthetic catch-up frames: each phase step requires a real vblank.
     * Backend must hold a real vblank reference for the whole transaction.
     */
    if (s->sequence_valid && (rq_u32)(sequence - s->last_sequence) != 1)
        return RKQMS_EIO;
    memset(a, 0, sizeof(*a));
    a->generation = s->generation;
    a->revision = s->revision;
    a->sequence = sequence;
    a->next = *s;
    a->next.sequence_valid = 1;
    a->next.last_sequence = sequence;
    phase = s->phase;
    switch (phase) {
    case RKQMS_BASE_CONST:
        packet(s, a, s->base_tfr, 1);
        if (++a->next.phase_frames == 2) {
            a->next.phase = RKQMS_BASE_VARIABLE; a->next.phase_frames = 0;
        }
        break;
    case RKQMS_BASE_VARIABLE:
        packet(s, a, s->current_tfr, 0);
        a->next.phase = RKQMS_TARGET_NOTICE;
        break;
    case RKQMS_TARGET_NOTICE:
        packet(s, a, s->requested_tfr, 0);
        a->next.phase = RKQMS_APPLY;
        break;
    case RKQMS_APPLY:
        packet(s, a, s->requested_tfr, 0);
        ret = timing(s, a, s->requested_tfr, 0);
        if (ret) return ret;
        a->next.current_tfr = s->requested_tfr;
        a->next.fraction_frame = 1;
        a->next.phase = RKQMS_CONFIRM;
        break;
    case RKQMS_CONFIRM: case RKQMS_STABLE:
        packet(s, a, s->current_tfr, 1);
        ret = timing(s, a, s->current_tfr, s->fraction_frame);
        if (ret) return ret;
        /* Keep the phase in a bounded cycle; no u32 overflow discontinuity. */
        a->next.fraction_frame = (s->fraction_frame + 1) %
            (rkqms_timing(s->vic, s->current_tfr)->bits ? rkqms_timing(s->vic, s->current_tfr)->bits : 1);
        a->next.phase = RKQMS_STABLE;
        break;
    case RKQMS_RESTORE_NOTICE:
        packet(s, a, s->base_tfr, 0);
        a->next.phase = RKQMS_RESTORE_APPLY;
        break;
    case RKQMS_RESTORE_APPLY:
        packet(s, a, s->base_tfr, 0);
        ret = timing(s, a, s->base_tfr, 0);
        if (ret) return ret;
        a->next.current_tfr = s->base_tfr;
        a->next.phase = RKQMS_RESTORE_SETTLE;
        a->next.phase_frames = 0;
        break;
    case RKQMS_RESTORE_SETTLE:
        packet(s, a, s->base_tfr, 0);
        if (++a->next.phase_frames == 2) a->next.phase = RKQMS_RESTORE_CONST;
        break;
    case RKQMS_RESTORE_CONST:
        packet(s, a, s->base_tfr, 1);
        a->next.phase = RKQMS_RESTORE_OFF;
        break;
    case RKQMS_RESTORE_OFF:
        packet(s, a, 0, 0);
        a->next.phase = RKQMS_OFF;
        a->next.sequence_valid = 0;
        break;
    default: return RKQMS_EINVAL;
    }
    a->next.packet_tfr = a->next_tfr;
    a->next.packet_const = a->m_const;
    a->next.vtem_enabled = a->vtem_enabled;
    ++a->next.revision;
    return 0;
}

int rkqms_complete(struct rkqms_state *s, const struct rkqms_action *a, int hardware_error)
{
    if (!s || !a || s->generation != a->generation || s->revision != a->revision)
        return RKQMS_ESTALE;
    if (hardware_error) {
        s->phase = RKQMS_FAULT;
        s->error = hardware_error < 0 ? hardware_error : RKQMS_EIO;
        ++s->revision;
        return s->error;
    }
    *s = a->next;
    return 0;
}

void rkqms_invalidate(struct rkqms_state *s, rq_u64 new_generation)
{
    if (!s) return;
    ++s->revision;
    s->generation = new_generation;
    s->phase = RKQMS_FAULT;
    s->error = RKQMS_ESTALE;
    /* Does not claim hardware restoration. Backend must quiesce HW first. */
}
