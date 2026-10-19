/* SPDX-License-Identifier: GPL-2.0-only */
#include "../fixture.h"
extern int fixture_irq;
static inline int in_interrupt(void){return fixture_irq;}
#define current fixture_current
