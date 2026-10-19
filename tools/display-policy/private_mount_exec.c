// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#include <sched.h>
#include <sys/mount.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Test runner only. Never mount in the caller's namespace or propagate mounts. */
int main(int argc, char **argv)
{
    char before[128] = {0}, after[128] = {0};
    if (argc < 3 || argc > 16) return 2;
    if (readlink("/proc/self/ns/mnt", before, sizeof(before) - 1) < 0 ||
        unshare(CLONE_NEWNS) ||
        readlink("/proc/self/ns/mnt", after, sizeof(after) - 1) < 0 ||
        !strcmp(before, after)) {
        perror("create separate mount namespace"); return 3;
    }
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)) {
        perror("prevent mount propagation"); return 4;
    }
    char *args[18] = {"sh"};
    for (int i = 1; i < argc; ++i) args[i] = argv[i];
    execv("/system/bin/sh", args);
    perror("exec private child"); return 5;
}
