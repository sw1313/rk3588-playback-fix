/* SPDX-License-Identifier: GPL-2.0-only */
/* Fixed syscall mode DTO only. Not evidence for native kernel struct layout. */
#ifndef RKQMS_MODE_FIXTURE_H
#define RKQMS_MODE_FIXTURE_H
#include <stdint.h>
struct drm_mode_modeinfo {
 uint32_t clock;uint16_t hdisplay,hsync_start,hsync_end,htotal,hskew;
 uint16_t vdisplay,vsync_start,vsync_end,vtotal,vscan;
 uint32_t vrefresh,flags,type;char name[32];
};
#endif
