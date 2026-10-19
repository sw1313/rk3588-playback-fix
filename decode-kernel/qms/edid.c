// SPDX-License-Identifier: GPL-2.0-only
// CTA field positions cross-checked with v4l-utils edid-decode HDMI Forum parser.
#include "edid.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

int rkqms_parse_edid(const rq_u8 *data, unsigned int bytes, struct rkqms_sink *sink)
{
    static const rq_u8 header[] = {0, 255, 255, 255, 255, 255, 255, 0};
    unsigned int b, i, count, matches = 0;
    struct rkqms_sink parsed = {0};
    if (!data || !sink || bytes < 128 || bytes % 128 || bytes > 128 * 256 ||
        memcmp(data, header, sizeof(header)))
        return RKQMS_EINVAL;
    count = data[126] + 1;
    if (bytes != count * 128)
        return RKQMS_EINVAL;
    for (b = 0; b < count; ++b) {
        unsigned int sum = 0;
        const rq_u8 *block = data + b * 128;
        for (i = 0; i < 128; ++i) sum += block[i];
        if (sum & 255) return RKQMS_EINVAL;
        if (!b || block[0] != 2 || block[1] < 3) continue;
        if (!block[2]) continue;
        if (block[2] < 4 || block[2] > 127) return RKQMS_EINVAL;
        for (i = 4; i < block[2]; ) {
            unsigned int size = block[i] & 31;
            const rq_u8 *db = block + i;
            if (i + size + 1 > block[2]) return RKQMS_EINVAL;
            if (block[i] >> 5 == 3 && size >= 3 &&
                db[1] == 0xd8 && db[2] == 0x5d && db[3] == 0xc4) {
                /* Multiple/conflicting capability blocks require a fuller
                 * EDID merge policy; never select the more permissive one.
                 */
                if (++matches != 1 || size < 10 || db[4] != 1) return RKQMS_ENOTSUP;
                parsed.qms = !!(db[8] & 0x40);
                parsed.max_frl = db[7] >> 4;
                parsed.vrr_min = db[9] & 63;
                parsed.vrr_max = ((db[9] & 192) << 2) | db[10];
                if (size >= 11) {
                    parsed.qms_tfr_min = !!(db[11] & 0x10);
                    parsed.qms_tfr_max = !!(db[11] & 0x20);
                }
            }
            i += size + 1;
        }
    }
    if (!matches || !parsed.qms || !parsed.vrr_min || parsed.vrr_min > 48 ||
        parsed.vrr_max < 100 || parsed.vrr_max < parsed.vrr_min)
        return RKQMS_ENOTSUP;
    for (i = 1; i <= 13; ++i) {
        rq_u32 num, den;
        if (rkqms_rate(i, &num, &den)) return RKQMS_EINVAL;
        /* TFRmin extends cinema rates below VRRmin; TFRmax extends above60.
         * Intersect this with the actual BRR/link/backend at session creation.
         */
        if (!parsed.qms_tfr_min && num < (rq_u32)parsed.vrr_min * den) continue;
        if (num > (rq_u32)parsed.vrr_max * den) continue;
        if (!parsed.qms_tfr_max && num > 60U * den) continue;
        parsed.tfr_mask |= 1U << i;
    }
    *sink = parsed;
    return 0;
}
