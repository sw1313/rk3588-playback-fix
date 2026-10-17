// SPDX-License-Identifier: MIT
#ifndef CSF_GROUP_CORE_H
#define CSF_GROUP_CORE_H
#include <stdint.h>
#define GROUP_CREATE 0xc020802aul
#define QUEUE_BIND 0xc0108027ul
#define GROUP_TERM 0x4008802bul
struct csf_group { int fd; uint32_t owner; uint8_t handle, created, refs, bound, closed, used; uint8_t output[8]; };
struct csf_groups { struct csf_group group[32]; };
typedef int (*csf_call)(void *, int, unsigned long, void *);
int csf_group_ioctl(struct csf_groups *, uint32_t, csf_call, void *, int, unsigned long, void *);
void csf_group_forget_fd(struct csf_groups *, int);
#endif
