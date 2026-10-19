/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_REGMAP_FIXTURE_H
#define RKQMS_REGMAP_FIXTURE_H
#include "../fixture.h"
struct device;
struct regmap {u32 words[1024];unsigned int reads,writes;int read_error,write_error,ignore_write;struct device *device;};
static inline struct device *regmap_get_device(struct regmap *m){return m->device;}
static inline int regmap_get_val_bytes(struct regmap *m){(void)m;return 4;}
static inline int regmap_get_reg_stride(struct regmap *m){(void)m;return 4;}
static inline int regmap_read(struct regmap *m,u32 offset,u32 *v){++m->reads;if(m->read_error)return m->read_error;*v=m->words[offset/4];return 0;}
static inline int regmap_write(struct regmap *m,u32 offset,u32 v){++m->writes;if(m->write_error)return m->write_error;if(!m->ignore_write)m->words[offset/4]=v;return 0;}
static inline int regmap_update_bits(struct regmap *m,u32 offset,u32 mask,u32 v){return regmap_write(m,offset,(m->words[offset/4]&~mask)|(v&mask));}
#endif
