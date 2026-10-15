/* SPDX-License-Identifier: Apache-2.0 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "rk_mpi.h"

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: probe library symbol-list expected-revision\n"); return 2; }
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 3; }
    FILE *symbols = fopen(argv[2], "r");
    if (!symbols) { fprintf(stderr, "symbol list unavailable\n"); dlclose(library); return 4; }
    char name[512]; int count = 0, missing = 0;
    while (fgets(name, sizeof(name), symbols)) {
        name[strcspn(name, "\r\n")] = '\0';
        if (!name[0]) continue;
        dlerror(); void *symbol = dlsym(library, name); const char *error = dlerror();
        if (!symbol || error) { fprintf(stderr, "missing symbol: %s\n", name); missing++; }
        count++;
    }
    fclose(symbols);
    if (missing || !count) { dlclose(library); return 5; }
    if (!strcmp(argv[3], "-")) {
        printf("Library symbols available: %d\n", count);
        dlclose(library);
        return 0;
    }
    const char *(*version)(void) = (const char *(*)(void)) dlsym(library, "get_mpp_version");
    const char *revision = version ? version() : NULL;
    if (!revision || !strstr(revision, argv[3])) { fprintf(stderr, "unexpected library revision\n"); dlclose(library); return 6; }
    MPP_RET (*create)(MppCtx *, MppApi **) = (MPP_RET (*)(MppCtx *, MppApi **)) dlsym(library, "mpp_create");
    MPP_RET (*destroy)(MppCtx) = (MPP_RET (*)(MppCtx)) dlsym(library, "mpp_destroy");
    if (!create || !destroy) { dlclose(library); return 7; }
    MppCtx context = NULL; MppApi *api = NULL;
    MPP_RET result = create(&context, &api);
    if (result || !context || !api) { fprintf(stderr, "MPP context creation failed\n"); if (context) destroy(context); dlclose(library); return 8; }
    printf("MPP revision: %s\nSymbols: %d; API size: %u; API version: %u\n", revision, count, api->size, api->version);
    result = destroy(context);
    dlclose(library);
    return result ? 9 : 0;
}
