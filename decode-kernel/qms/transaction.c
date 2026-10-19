// SPDX-License-Identifier: GPL-2.0-only
#include "transaction.h"

int rkqms_frame(struct rkqms_transaction *t, rq_u64 generation, rq_u32 sequence)
{
    struct rkqms_action a;
    int ret;
    unsigned int i;
    if (!t || !t->io.begin || !t->io.end || !t->io.write_packet ||
        !t->io.update_packet_enable || !t->io.commit_timing)
        return RKQMS_ENOTSUP;
    if (t->needs_native_restore)
        return t->last_error ? t->last_error : RKQMS_EIO;
    ret = rkqms_plan(&t->core, generation, sequence, &a);
    if (ret) {
        if (ret == RKQMS_EIO) {
            t->needs_native_restore = 1;
            t->last_error = ret;
        }
        return ret;
    }
    ret = t->io.begin(t->io.context, generation, sequence);
    if (ret) {
        /* begin() itself must unwind on failure. A missed real frame cannot
         * be replaced by a synthetic successful state-machine step.
         */
        t->needs_native_restore = 1;
        t->last_error = ret;
        return ret;
    }
    /* Upstream payload ordering: fill VTEM words, then update only its TX_EN
     * bit. Preserve CVTEM/HDR/audio scheduler bits. No ignore-error writes.
     */
    if (a.write_vtem && a.vtem_enabled) {
        for (i = 0; i < 8; ++i) {
            ret = t->io.write_packet(t->io.context, 0xd00 + i * 4, a.vtem[i]);
            if (ret) goto out;
        }
    }
    if (a.write_vtem) {
        ret = t->io.update_packet_enable(t->io.context, 1U << 9, a.vtem_enabled ? 1U << 9 : 0);
        if (ret) goto out;
    }
    if (a.write_timing) {
        if (!a.vtotal || a.vtotal > 0x1fff) {
            ret = RKQMS_EINVAL;
            goto out;
        }
        ret = t->io.commit_timing(t->io.context, a.vtotal, a.vfp);
        if (ret) goto out;
    }
out:
    t->io.end(t->io.context);
    ret = rkqms_complete(&t->core, &a, ret);
    if (ret) {
        /* A failed write may have reached hardware. The caller must use the
         * factory full modeset/reset path, not claim rollback or seamlessness.
         */
        t->needs_native_restore = 1;
        t->last_error = ret;
    }
    return ret;
}
