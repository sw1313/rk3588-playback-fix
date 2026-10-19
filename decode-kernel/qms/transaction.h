/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_TRANSACTION_H
#define RKQMS_TRANSACTION_H
#include "core.h"
/* These are INTERNAL binding operations, never raw userspace register input.
 * begin() must serialize with the factory driver's complete atomic commit,
 * verify the live connector/CRTC/generation/power/link and provide a safe
 * programming window. A spinlock alone is not that contract.
 * commit_timing() must preserve ordinary planes/fences and include cfg_done.
 * end() must release every lock/reference acquired by begin(), including errors.
 */
struct rkqms_io {
    void *context;
    int (*begin)(void *, rq_u64 generation, rq_u32 frame);
    int (*write_packet)(void *, rq_u32 offset, rq_u32 value);
    int (*update_packet_enable)(void *, rq_u32 mask, rq_u32 value);
    int (*commit_timing)(void *, rq_u16 vtotal, rq_u16 vfp);
    void (*end)(void *);
};
struct rkqms_transaction {
    struct rkqms_state core;
    struct rkqms_io io;
    rq_u8 needs_native_restore;
    int last_error;
};
int rkqms_frame(struct rkqms_transaction *, rq_u64 generation, rq_u32 sequence);
#endif
