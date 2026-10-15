#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpp_frame.h"
MPP_RET mpp_frame_copy(MppFrame dst, MppFrame src);

#define LOAD(name) __typeof__(&name) fn_##name = dlsym(lib, #name); if (!fn_##name) return 3
#define REQUIRE(cond) do { if (!(cond)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); return 1; } } while (0)
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { puts(dlerror()); return 3; }
    LOAD(mpp_frame_init); LOAD(mpp_frame_deinit); LOAD(mpp_frame_copy);
    LOAD(mpp_frame_get_hdr_dynamic_meta); LOAD(mpp_frame_set_hdr_dynamic_meta);
    MppFrame (*duplicate)(MppFrame) = dlsym(lib, "mpp_frame_dup");
    REQUIRE(duplicate);
    struct { uint32_t hdr_fmt, size; uint8_t bytes[32]; } input = {8, 32, {0}};
    for (unsigned i = 0; i < 20000; ++i) {
        MppFrame a = NULL, b = NULL;
        fn_mpp_frame_init(&a); fn_mpp_frame_init(&b);
        REQUIRE(fn_mpp_frame_get_hdr_dynamic_meta(a) == NULL);
        memset(input.bytes, i & 255, 32);
        fn_mpp_frame_set_hdr_dynamic_meta(a, (MppFrameHdrDynamicMeta *)&input);
        MppFrameHdrDynamicMeta *p = fn_mpp_frame_get_hdr_dynamic_meta(a);
        REQUIRE(p && p != (void *)&input && p->size == 32 && p->data[0] == (i & 255));
        memset(input.bytes, 0xa5, 32);
        REQUIRE(p->data[0] == (i & 255));
        fn_mpp_frame_set_hdr_dynamic_meta(a, p);
        REQUIRE(fn_mpp_frame_get_hdr_dynamic_meta(a)->data[0] == (i & 255));
        REQUIRE(fn_mpp_frame_copy(b, a) == 0);
        REQUIRE(fn_mpp_frame_copy(a, a) == 0);
        MppFrame c = duplicate(a);
        REQUIRE(c != NULL);
        fn_mpp_frame_deinit(&a);
        REQUIRE(fn_mpp_frame_get_hdr_dynamic_meta(b)->data[0] == (i & 255));
        fn_mpp_frame_set_hdr_dynamic_meta(b, NULL);
        REQUIRE(fn_mpp_frame_get_hdr_dynamic_meta(b) == NULL);
        fn_mpp_frame_deinit(&b);
        REQUIRE(fn_mpp_frame_get_hdr_dynamic_meta(c)->data[0] == (i & 255));
        fn_mpp_frame_deinit(&c);
    }
    puts("PASS 20000 HDR deep-copy, duplicate, self-copy, alias-set, clear and recycle cycles");
    return 0;
}
