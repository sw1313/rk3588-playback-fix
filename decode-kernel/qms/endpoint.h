/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_ENDPOINT_H
#define RKQMS_ENDPOINT_H
#include "controller.h"
/* Register only after a validated native binding supplied ALL proof bits and
 * a running controller. This source is not called by the research probe.
 * Permissions/SELinux and the boot receipt are completed by Decode's loader.
 * Unregister denies new ioctls before draining. Functional native hooks must
 * remain pinned until reboot even after this endpoint has been removed.
 */
int rkqms_endpoint_register(struct rkqms_controller *);
void rkqms_endpoint_unregister(void);
#endif
