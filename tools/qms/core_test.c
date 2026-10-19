// SPDX-License-Identifier: GPL-2.0-only
/* Executes the actual production core, with no hardware backend. */
#include "core.h"
#include "edid.h"
#include "transaction.h"
#include <stddef.h>

void *memset(void *dest, int value, size_t size)
{
    unsigned char *p = dest;
    while (size--) *p++ = value;
    return dest;
}
void *memcpy(void *dest, const void *source, size_t size)
{
    unsigned char *p = dest;
    const unsigned char *q = source;
    while (size--) *p++ = *q++;
    return dest;
}
int memcmp(const void *left, const void *right, size_t size)
{
    const unsigned char *a = left, *b = right;
    while (size--) { if (*a != *b) return *a < *b ? -1 : 1; ++a; ++b; }
    return 0;
}
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
struct results { unsigned int table_cycles, switches, stable_frames, negative_cases,
                             transaction_faults, edid_cases; } test_results;

static struct rkqms_mode mode(unsigned int vic)
{
    struct rkqms_mode m = {0};
    m.flags = 5;
    if (vic == 4 || vic == 47) {
        m.clock_khz = vic == 4 ? 74250 : 148500;
        m.width = 1280; m.height = 720; m.htotal = 1650; m.vtotal = 750;
        m.vsync_start = 725; m.vsync_end = 730;
    } else if (vic == 16 || vic == 63) {
        m.clock_khz = vic == 16 ? 148500 : 297000;
        m.width = 1920; m.height = 1080; m.htotal = 2200; m.vtotal = 1125;
        m.vsync_start = 1084; m.vsync_end = 1089;
    } else {
        m.clock_khz = 594000; m.width = vic == 97 ? 3840 : 4096;
        m.height = 2160; m.htotal = 4400; m.vtotal = 2250;
        m.vsync_start = 2168; m.vsync_end = 2178;
    }
    return m;
}

static int exact_cycles(void)
{
    static const unsigned int vics[] = {4, 47, 16, 63, 97, 102};
    unsigned int i, t, frame, length;
    rq_u32 n, d, base_rate;
    rq_u64 sum;
    for (i = 0; i < sizeof(vics) / sizeof(vics[0]); ++i) {
        struct rkqms_mode m = mode(vics[i]);
        base_rate = vics[i] == 47 || vics[i] == 63 ? 120 : 60;
        for (t = 1; t <= 13; ++t) {
            const struct rkqms_timing *timing = rkqms_timing(vics[i], t);
            if (!timing) continue;
            CHECK(!rkqms_rate(t, &n, &d));
            length = timing->bits ? timing->bits : 1;
            sum = 0;
            for (frame = 0; frame < length; ++frame) {
                rq_u16 total = rkqms_vtotal(timing, frame);
                CHECK(total == timing->vtotal || total == timing->vtotal + 1);
                sum += total;
            }
            /* Exact rational rate including 1000/1001; no decimal truncation. */
            CHECK(sum * n == (rq_u64)m.vtotal * base_rate * d * length);
            ++test_results.table_cycles;
        }
    }
    CHECK(!rkqms_timing(255, 2));
    CHECK(!rkqms_timing(97, 13));
    CHECK(!rkqms_timing(4, 255));
    return 0;
}

static int step(struct rkqms_state *s, unsigned int *seq, struct rkqms_action *a)
{
    CHECK(!rkqms_plan(s, s->generation, ++*seq, a));
    CHECK(a->next.generation == s->generation);
    CHECK(a->write_timing == (a->vtotal && a->vtotal != s->current_vtotal));
    if (a->write_timing) {
        CHECK(a->vtotal >= s->base.vtotal);
        CHECK(a->vfp == s->base.vsync_start - s->base.height + a->vtotal - s->base.vtotal);
    }
    CHECK(!rkqms_complete(s, a, 0));
    return 0;
}

static int switching(void)
{
    static const unsigned int vics[] = {4, 47, 16, 63, 97, 102};
    struct rkqms_caps caps = {0x3ffe, 1, 1};
    unsigned int i, t, u, frame, seq;
    int ret;
    for (i = 0; i < sizeof(vics) / sizeof(vics[0]); ++i) {
        struct rkqms_mode m = mode(vics[i]);
        for (t = 1; t <= 13; ++t) {
            if (!rkqms_timing(vics[i], t)) continue;
            for (u = 1; u <= 13; ++u) {
                struct rkqms_state s;
                struct rkqms_action a;
                if (!rkqms_timing(vics[i], u)) continue;
                CHECK(!rkqms_init(&s, &m, vics[i], &caps, 99));
                seq = 0xfffffffbu; /* Exercise vblank sequence wrap. */
                CHECK(!rkqms_request(&s, t, 99, 1));
                for (frame = 0; frame < 6; ++frame) {
                    ret = step(&s, &seq, &a); if (ret) return ret;
                    if (frame < 4) CHECK(!a.write_timing);
                    if (frame < 2) CHECK(a.next_tfr == s.base_tfr && a.m_const);
                    if (frame == 3) CHECK(a.next_tfr == t && !a.m_const);
                }
                CHECK(s.phase == RKQMS_STABLE && s.current_tfr == t);
                CHECK(a.vtem[0] == 0xc000 && a.vtem[1] == 0xc6 && a.vtem[2] == 0x06040000);
                CHECK(a.vtem[3] == ((s.base_tfr == 13 ? 120U : 60U) << 16 | t << 11 |
                      (s.base.vsync_start - s.base.height)));
                for (frame = 0; frame < 128; ++frame) {
                    ret = step(&s, &seq, &a); if (ret) return ret;
                    CHECK(s.phase == RKQMS_STABLE && !a.write_vtem);
                    ++test_results.stable_frames;
                }
                CHECK(!rkqms_request(&s, u, 99, 2));
                for (frame = 0; frame < (t == u ? 1U : 4U); ++frame) {
                    ret = step(&s, &seq, &a); if (ret) return ret;
                }
                CHECK(s.phase == RKQMS_STABLE && s.current_tfr == u);
                CHECK(!rkqms_request(&s, 0, 99, 3));
                for (frame = 0; frame < 6; ++frame) {
                    ret = step(&s, &seq, &a); if (ret) return ret;
                    if (frame < 5) CHECK(a.vtem_enabled);
                }
                CHECK(s.phase == RKQMS_OFF && !s.vtem_enabled && s.current_vtotal == m.vtotal);
                CHECK(rkqms_plan(&s, 99, seq + 1, &a) == RKQMS_EAGAIN);
                ++test_results.switches;
            }
        }
    }
    return 0;
}

static int failures(void)
{
    struct rkqms_state s, before;
    struct rkqms_action a;
    struct rkqms_mode m = mode(97);
    struct rkqms_caps caps = {0x3ffe, 1, 1};
    unsigned int seq = 0;
    int ret;
    CHECK(!rkqms_init(&s, &m, 97, &caps, 10));
    before = s;
    CHECK(rkqms_request(&s, 2, 11, 1) == RKQMS_ESTALE);
    CHECK(s.revision == before.revision);
    CHECK(rkqms_request(&s, 255, 10, 1) == RKQMS_ENOTSUP);
    CHECK(rkqms_request(&s, 13, 10, 1) == RKQMS_ENOTSUP);
    CHECK(!rkqms_request(&s, 2, 10, 1));
    CHECK(rkqms_request(&s, 3, 10, 2) == RKQMS_EBUSY);
    CHECK(!rkqms_plan(&s, 10, 1, &a));
    CHECK(!s.vtem_enabled); /* plan has not published or touched hardware. */
    CHECK(rkqms_complete(&s, &a, -12) == -12);
    CHECK(s.phase == RKQMS_FAULT && !s.vtem_enabled);
    CHECK(rkqms_request(&s, 0, 10, 3) == -12);
    CHECK(!rkqms_init(&s, &m, 97, &caps, 11));
    CHECK(!rkqms_request(&s, 2, 11, 4));
    CHECK(!rkqms_plan(&s, 11, 1, &a));
    rkqms_invalidate(&s, 12);
    CHECK(rkqms_complete(&s, &a, 0) == RKQMS_ESTALE);
    CHECK(s.phase == RKQMS_FAULT && s.generation == 12);
    CHECK(!rkqms_init(&s, &m, 97, &caps, 13));
    CHECK(!rkqms_request(&s, 2, 13, 5));
    ret = step(&s, &seq, &a); if (ret) return ret;
    CHECK(rkqms_plan(&s, 13, seq, &a) == RKQMS_EIO);
    CHECK(rkqms_plan(&s, 13, seq + 2, &a) == RKQMS_EIO);
    caps.qms = 0;
    CHECK(rkqms_init(&s, &m, 97, &caps, 14) == RKQMS_ENOTSUP);
    caps.qms = 1; caps.unchanged_link = 0;
    CHECK(rkqms_init(&s, &m, 97, &caps, 14) == RKQMS_ENOTSUP);
    caps.unchanged_link = 1; m.flags |= 16;
    CHECK(rkqms_init(&s, &m, 97, &caps, 14) == RKQMS_ENOTSUP);
    m = mode(97); m.clock_khz = 593407;
    CHECK(rkqms_init(&s, &m, 97, &caps, 14) == RKQMS_ENOTSUP);
    test_results.negative_cases = 15;
    return 0;
}

struct fake_io {
    unsigned int writes, fail_at, starts, ends, locked;
    rq_u32 scheduler, packet[8];
    rq_u16 vtotal, vfp;
    int begin_error;
};
static int begin(void *p, rq_u64 generation, rq_u32 sequence)
{
    struct fake_io *f = p;
    (void)generation; (void)sequence;
    ++f->starts;
    if (f->begin_error) return f->begin_error;
    f->locked = 1;
    return 0;
}
static int write_packet(void *p, rq_u32 offset, rq_u32 value)
{
    struct fake_io *f = p;
    if (!f->locked || offset < 0xd00 || offset > 0xd1c || offset % 4) return -22;
    f->packet[(offset - 0xd00) / 4] = value;
    return ++f->writes == f->fail_at ? -5 : 0;
}
static int scheduler(void *p, rq_u32 mask, rq_u32 value)
{
    struct fake_io *f = p;
    if (!f->locked || mask != 1U << 9) return -22;
    f->scheduler = (f->scheduler & ~mask) | (value & mask);
    return ++f->writes == f->fail_at ? -5 : 0;
}
static int commit_timing(void *p, rq_u16 total, rq_u16 vfp)
{
    struct fake_io *f = p;
    if (!f->locked) return -22;
    f->vtotal = total; f->vfp = vfp;
    return ++f->writes == f->fail_at ? -5 : 0;
}
static void end(void *p)
{
    struct fake_io *f = p;
    f->locked = 0; ++f->ends;
}
static void io_setup(struct rkqms_transaction *t, struct fake_io *f)
{
    struct rkqms_mode m = mode(97);
    struct rkqms_caps caps = {0x7fe, 1, 1};
    memset(t, 0, sizeof(*t)); memset(f, 0, sizeof(*f));
    rkqms_init(&t->core, &m, 97, &caps, 7);
    f->scheduler = 0x6400; f->vtotal = 2250;
    t->io.context = f; t->io.begin = begin; t->io.end = end;
    t->io.write_packet = write_packet; t->io.update_packet_enable = scheduler;
    t->io.commit_timing = commit_timing;
}
static int transactions(void)
{
    struct rkqms_transaction t;
    struct fake_io f;
    unsigned int fail, seq;
    for (fail = 1; fail <= 9; ++fail) {
        rq_u64 revision;
        io_setup(&t, &f); f.fail_at = fail;
        CHECK(!rkqms_request(&t.core, 1, 7, 1)); revision = t.core.revision;
        CHECK(rkqms_frame(&t, 7, 1) == -5);
        CHECK(t.needs_native_restore && t.core.phase == RKQMS_FAULT);
        CHECK(!t.core.vtem_enabled && t.core.revision == revision + 1);
        CHECK(f.writes == fail && !f.locked && f.starts == 1 && f.ends == 1);
        CHECK(rkqms_frame(&t, 7, 2) == -5 && f.writes == fail);
        CHECK((f.scheduler & ~0x200U) == 0x6400);
        ++test_results.transaction_faults;
    }
    io_setup(&t, &f); f.begin_error = -11;
    CHECK(!rkqms_request(&t.core, 1, 7, 1));
    CHECK(rkqms_frame(&t, 7, 1) == -11 && !f.writes && !f.ends && !f.locked);
    CHECK(t.needs_native_restore);
    ++test_results.transaction_faults;
    io_setup(&t, &f);
    CHECK(!rkqms_request(&t.core, 1, 7, 1));
    for (seq = 1; seq <= 4; ++seq) CHECK(!rkqms_frame(&t, 7, seq));
    f.fail_at = f.writes + 1;
    CHECK(rkqms_frame(&t, 7, 5) == -5); /* Actual timing transaction error. */
    CHECK(t.needs_native_restore && t.core.current_tfr == 10 && !f.locked);
    ++test_results.transaction_faults;
    io_setup(&t, &f);
    CHECK(!rkqms_request(&t.core, 1, 7, 1));
    for (seq = 1; seq <= 134; ++seq) CHECK(!rkqms_frame(&t, 7, seq));
    CHECK(t.core.phase == RKQMS_STABLE && t.core.current_tfr == 1 && !t.needs_native_restore);
    CHECK((f.scheduler & ~0x200U) == 0x6400);
    CHECK(!rkqms_request(&t.core, 0, 7, 2));
    for (; seq <= 140; ++seq) CHECK(!rkqms_frame(&t, 7, seq));
    CHECK(t.core.phase == RKQMS_OFF && f.scheduler == 0x6400 && f.vtotal == 2250);
    CHECK(!f.locked && f.starts == f.ends);
    return 0;
}

static void checksum(unsigned char *block)
{
    unsigned int i, sum = 0;
    block[127] = 0;
    for (i = 0; i < 127; ++i) sum += block[i];
    block[127] = (unsigned char)(0U - sum);
}
static int edids(void)
{
    unsigned char data[256] = {0};
    struct rkqms_sink sink;
    unsigned int i;
    for (i = 1; i < 7; ++i) data[i] = 255;
    data[126] = 1; checksum(data);
    data[128] = 2; data[129] = 3; data[130] = 16;
    data[132] = (3U << 5) | 11;
    data[133] = 0xd8; data[134] = 0x5d; data[135] = 0xc4; data[136] = 1;
    data[140] = 0x40; data[141] = 48; data[142] = 120; data[143] = 0x10;
    checksum(data + 128);
    CHECK(!rkqms_parse_edid(data, sizeof(data), &sink));
    CHECK(sink.qms && sink.qms_tfr_min && !sink.qms_tfr_max && sink.tfr_mask == 0x7fe);
    data[143] = 0; checksum(data + 128);
    CHECK(!rkqms_parse_edid(data, sizeof(data), &sink));
    CHECK(sink.tfr_mask == ((1U << 7) | (1U << 8) | (1U << 9) | (1U << 10)));
    data[143] = 0x30; checksum(data + 128);
    CHECK(!rkqms_parse_edid(data, sizeof(data), &sink) && sink.tfr_mask == 0x3ffe);
    data[140] = 0; checksum(data + 128);
    CHECK(rkqms_parse_edid(data, sizeof(data), &sink) == RKQMS_ENOTSUP);
    data[140] = 0x40; data[141] = 49; checksum(data + 128);
    CHECK(rkqms_parse_edid(data, sizeof(data), &sink) == RKQMS_ENOTSUP);
    data[141] = 48; checksum(data + 128);
    ++data[200];
    CHECK(rkqms_parse_edid(data, sizeof(data), &sink) == RKQMS_EINVAL);
    --data[200]; data[130] = 8; checksum(data + 128);
    CHECK(rkqms_parse_edid(data, sizeof(data), &sink) == RKQMS_EINVAL);
    CHECK(rkqms_parse_edid(data, 255, &sink) == RKQMS_EINVAL);
    CHECK(rkqms_parse_edid(data, 128, &sink) == RKQMS_EINVAL);
    CHECK(rkqms_parse_edid(data, 0, &sink) == RKQMS_EINVAL);
    test_results.edid_cases = 10;
    return 0;
}

int main(void)
{
    int ret;
    ret = exact_cycles(); if (ret) return ret;
    ret = switching(); if (ret) return ret;
    ret = failures(); if (ret) return ret;
    ret = transactions(); if (ret) return ret;
    return edids();
}
