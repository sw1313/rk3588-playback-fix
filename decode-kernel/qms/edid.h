/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_EDID_H
#define RKQMS_EDID_H
#include "core.h"
struct rkqms_sink {
    rq_u32 tfr_mask;
    rq_u16 vrr_min, vrr_max;
    rq_u8 qms, qms_tfr_min, qms_tfr_max, max_frl;
};
/* Bounded CTA HDMI Forum VSDB parser. Unsupported HF-SCDB-only EDIDs are not
 * promoted to supported. Does not infer a current link format or link safety.
 */
int rkqms_parse_edid(const rq_u8 *data, unsigned int bytes, struct rkqms_sink *sink);
#endif
