/* SPDX-License-Identifier: GPL-2.0-only */
#include "../fixture.h"
#define __iomem
#define BIT(n) (1U<<(n))
extern unsigned int fixture_mmio_reads,fixture_mmio_writes;
static inline u32 readl(const void *p){++fixture_mmio_reads;return *(const u32 *)p;}
static inline void writel(u32 value,void *p){++fixture_mmio_writes;*(u32 *)p=value;}
