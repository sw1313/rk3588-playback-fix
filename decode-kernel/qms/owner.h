/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_OWNER_H
#define RKQMS_OWNER_H
struct device;
int rkqms_owner_start(unsigned long anchor,struct device **devices,unsigned long drm,
                      unsigned long hdmi,unsigned long qp,unsigned long vop);
bool rkqms_owner_published(void);
#ifdef RKQMS_OWNER_INSPECT
int rkqms_owner_inspect(unsigned long anchor,struct device **devices,unsigned long drm,
                       unsigned long hdmi,unsigned long qp,unsigned long vop);
#endif
#endif
