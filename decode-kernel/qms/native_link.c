// SPDX-License-Identifier: GPL-2.0-only
#include <linux/device.h>
#include "native_link.h"

static int read_at(void *to,unsigned long p,unsigned int bytes)
{
    return p && !copy_from_kernel_nofault(to,(void *)p,bytes) ? 0 : -EFAULT;
}

/* Retain the immutable native blob by its real DRM ID before copying. This
 * works for both EDID and HDR; no raw blob pointer is kept after put(). The
 * caller holds mode_config.mutex via native drm_modeset_lock_all().
 */
static int blob_copy(struct drm_device *drm,unsigned long blob,void *to,
                     unsigned int capacity,unsigned int *bytes)
{
    struct drm_property_blob *held;
    unsigned long length,data;
    u32 id;
    int ret=-ESTALE;
    if (!blob || read_at(&id,blob,4) || !id) return -ESTALE;
    held=drm_property_lookup_blob(drm,id);
    if (!held) return -ESTALE;
    if ((unsigned long)held==blob && !read_at(&length,blob+0x48,8) &&
        length && length<=capacity && rkqms_reg_ptr(blob+0x50,&data) && data) {
        ret=read_at(to,data,length);
        if (!ret) *bytes=length;
    }
    drm_property_blob_put(held);return ret;
}

/* Only formats whose output clock arithmetic is understood in this profile.
 * Reject other formats rather than reporting a guessed precision. This does
 * not ask the factory to change the format or lower its precision.
 */
static int format(u32 bus,u32 *bpc,u32 *avi_y)
{
    switch (bus) {
    case 0x100a:*bpc=8;*avi_y=0;break; /* RGB888_1X24 */
    case 0x1018:*bpc=10;*avi_y=0;break;
    case 0x201e:*bpc=8;*avi_y=2;break; /* YUV8_1X24 */
    case 0x2016:*bpc=10;*avi_y=2;break;
    case 0x2026:*bpc=8;*avi_y=3;break; /* UYYVYY8_0_5X24 */
    case 0x2027:*bpc=10;*avi_y=3;break;
    default:return -EOPNOTSUPP; /* 422 needs separate native packing proof. */
    }
    return 0;
}
static int infoframe(const u32 *words,unsigned int type,unsigned int version,
                     unsigned int length,u8 *payload)
{
    unsigned int i,sum=type+version+length;
    if (words[0]!=((length<<16)|(version<<8))) return -ESTALE;
    for (i=0;i<=length;++i) {
        payload[i]=(words[1+i/4]>>(8*(i%4)))&255;sum+=payload[i];
    }
    return sum&255 ? -ESTALE : 0;
}

int rkqms_native_link_prepare(struct rkqms_native_link *l,struct drm_device *drm,
                 struct rkqms_registers *r,unsigned long hdmi,struct device *phy,
                 int (*reader)(void *,u8 *,unsigned int,size_t))
{
    unsigned long hdptx,parent,map,hpd,grf,clock,ddc,generic,owner;
    u8 id;
    if (l) l->qualified=false;
    if (!l || !r || !drm || !reader ||
        !rkqms_reg_ptr(hdmi+0x100,&parent) || parent!=r->qp ||
        !rkqms_reg_ptr(hdmi+0x90,&parent) || parent!=(unsigned long)drm ||
        read_at(&id,hdmi+0x118,1) || id ||
        !rkqms_reg_ptr((unsigned long)phy+0x78,&hdptx) || !hdptx ||
        !rkqms_reg_ptr(hdptx,&parent) || parent!=(unsigned long)phy ||
        !rkqms_reg_ptr(hdptx+8,&map) || !map ||
        regmap_get_device((void *)map)!=phy ||
        !rkqms_reg_ptr(hdptx+0x20,&generic) || !generic ||
        !rkqms_reg_ptr(generic+0x78,&owner) || owner!=hdptx ||
        /* Factory bind stores devm_phy_optional_get() at +108. +a0 is
         * platform data, not a PHY object. The full bind is fingerprinted. */
        !rkqms_reg_ptr(hdmi+0x108,&owner) || owner!=generic ||
        !rkqms_reg_ptr(hdmi+8,&hpd) || !hpd ||
        !rkqms_reg_ptr(hdptx+0x10,&grf) || !grf ||
        !rkqms_reg_ptr(hdptx+0x58,&clock) || !clock ||
        !rkqms_reg_ptr(r->qp+0xb30,&ddc) || !ddc ||
        regmap_get_val_bytes((void *)grf)!=4 || regmap_get_reg_stride((void *)grf)!=4 ||
        regmap_get_val_bytes((void *)hpd)!=4 || regmap_get_reg_stride((void *)hpd)!=4)
        return -ESTALE;
    memset(l,0,sizeof(*l));l->drm=drm;l->registers=r;l->hdmi=hdmi;l->hdptx=hdptx;
    l->hpd_map=(void *)hpd;l->phy_grf=(void *)grf;l->phy_clk=(void *)clock;
    l->ddc=(void *)ddc;l->read_edid_block=reader;return 0;
}

int rkqms_native_link_observe(struct rkqms_native_link *l,struct rkqms_link_snapshot *o)
{
    struct rkqms_registers *r;
    struct drm_mode_modeinfo mode;
    unsigned long state,connector_state,blob,native_bus;
    u8 powered,disabled,active,hdmi,avi[17],hdr[28];
    u32 hpd,y;unsigned int i,bytes=0;u64 expected_clock;
    int ret=-ESTALE;
    if (!l || !o || !l->registers) return -EINVAL;
    r=l->registers;memset(o,0,sizeof(*o));
    l->diagnostic=1;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    /* No clock-dependent registers before power and CRTC-active checks. */
    l->diagnostic=2;
    if (read_at(&powered,r->qp+0xb42,1) || !powered ||
        read_at(&disabled,r->qp+0xb74,1) || disabled ||
        read_at(&hdmi,r->qp+0xb40,1) || !hdmi ||
        !rkqms_reg_ptr((unsigned long)r->crtc+0x5c0,&state) ||
        read_at(&active,state+9,1) || !active) goto out;
    l->diagnostic=3;ret=regmap_read(l->hpd_map,0x384,&hpd);
    if (ret) goto out;
    if (!(hpd&BIT(16))) {ret=-ENOLINK;goto out;}
    l->diagnostic=4;ret=regmap_read(l->phy_grf,0x80,&o->phy_status);
    if (ret) goto out;
    /* Same two readiness bits required by factory TMDS power_on(). */
    if ((o->phy_status&0xa)!=0xa) {ret=-ENOLINK;goto out;}
    ret=-ESTALE;
    l->diagnostic=5;
    if (!rkqms_reg_ptr(r->qp+0x6d8,&connector_state) ||
        !rkqms_reg_ptr(connector_state+8,&blob) || blob!=(unsigned long)r->crtc ||
        read_at(&o->connector,r->qp+0x28,4) ||
        read_at(&o->crtc,(unsigned long)r->crtc+0x60,4) ||
        read_at(&o->edid_epoch,r->qp+0x608,8) ||
        !rkqms_reg_ptr(r->qp+0x160,&o->edid_blob)) goto out;
    l->diagnostic=6;
    if(!o->edid_blob) goto out;
    l->diagnostic=7;
    if(read_at(&o->bus_format,r->qp+0x83c,4) || /* +838 is INPUT! */
        !rkqms_reg_ptr(l->hdmi+0x128,&native_bus) || native_bus!=o->bus_format) goto out;
    l->diagnostic=8;
    if(read_at(&o->encoding,r->qp+0x844,4) ||
        read_at(&o->colorspace,connector_state+0x78,4) ||
        read_at(&o->pixel_clock,r->qp+0x858,8) ||
        read_at(&o->tmds_clock,r->qp+0x870,4)) goto out;
    memset(&mode,0,sizeof(mode));drm_mode_convert_to_umode(&mode,(void *)(state+0x18));
    o->mode.clock_khz=mode.clock;o->mode.flags=mode.flags;o->mode.width=mode.hdisplay;
    o->mode.height=mode.vdisplay;o->mode.htotal=mode.htotal;o->mode.vtotal=mode.vtotal;
    o->mode.vsync_start=mode.vsync_start;o->mode.vsync_end=mode.vsync_end;
    l->diagnostic=9;ret=format(o->bus_format,&o->bpc,&y);if (ret) goto out;
    l->diagnostic=10;
    ret=regmap_read(r->map,0x968,&o->link_config);if (ret) goto out;
    /* First profile is actual uncompressed TMDS. FRL needs trained-lane and
     * DSC state qualification; an EDID max_frl value is not that proof. */
    if (o->link_config) {ret=-EOPNOTSUPP;goto out;}
    l->diagnostic=11;ret=regmap_read(r->map,0xaa8,&o->scheduler);if (ret) goto out;
    ret=regmap_read(r->map,0x814,&o->interface_status);if (ret) goto out;
    for (i=0;i<5;++i) {ret=regmap_read(r->map,0xbe0+4*i,o->avi+i);if (ret) goto out;}
    l->diagnostic=12;ret=infoframe(o->avi,0x82,2,13,avi);if (ret) goto out;
    l->diagnostic=13;
    if (!(o->scheduler&BIT(13)) || ((avi[1]>>5)&7)!=y) {ret=-ESTALE;goto out;}
    l->diagnostic=14;expected_clock=(u64)mode.clock*1000;
    if (o->pixel_clock!=expected_clock) {ret=-ESTALE;goto out;}
    expected_clock=expected_clock*o->bpc/8;
    if (y==3) expected_clock/=2;
    l->diagnostic=15;o->phy_clock=clk_get_rate(l->phy_clk);
    if (o->tmds_clock!=expected_clock || o->phy_clock!=expected_clock) {ret=-ESTALE;goto out;}
    ret=-ESTALE;
    l->diagnostic=16;
    if (!rkqms_reg_ptr(connector_state+0x90,&blob)) goto out;
    if (blob) {
        l->diagnostic=17;ret=blob_copy(l->drm,blob,o->metadata,sizeof(o->metadata),&bytes);
        if (ret) goto out;
        if (bytes!=32 || o->metadata[0] || o->metadata[1] || o->metadata[2] ||
            o->metadata[3] || o->metadata[5] || o->metadata[4]>3) {ret=-EOPNOTSUPP;goto out;}
        o->eotf=o->metadata[4];
        if (o->eotf && o->bpc<10) {ret=-EOPNOTSUPP;goto out;}
        l->diagnostic=18;for (i=0;i<8;++i) {ret=regmap_read(r->map,0xc60+4*i,o->hdr+i);if (ret) goto out;}
        ret=infoframe(o->hdr,0x87,1,26,hdr);if (ret) goto out;
        if (!(o->scheduler&BIT(17)) || memcmp(hdr+1,o->metadata+4,26)) {ret=-ESTALE;goto out;}
    } else if (o->scheduler&BIT(17)) {ret=-ESTALE;goto out;}
    ret=0;
out:
    if(!ret)l->diagnostic=0;
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}

int rkqms_native_link_same(const struct rkqms_link_snapshot *a,const struct rkqms_link_snapshot *b)
{
    /* No padding comparisons, no dependence on unrelated audio enables. */
    return !memcmp(&a->mode,&b->mode,sizeof(a->mode)) &&
        a->edid_epoch==b->edid_epoch && a->edid_blob==b->edid_blob &&
        a->connector==b->connector && a->crtc==b->crtc &&
        a->bus_format==b->bus_format && a->encoding==b->encoding &&
        a->colorspace==b->colorspace && a->bpc==b->bpc && a->eotf==b->eotf &&
        a->link_config==b->link_config && a->interface_status==b->interface_status &&
        a->pixel_clock==b->pixel_clock && a->tmds_clock==b->tmds_clock && a->phy_clock==b->phy_clock &&
        !((a->scheduler^b->scheduler)&(BIT(13)|BIT(17)|BIT(10))) &&
        !memcmp(a->avi,b->avi,sizeof(a->avi)) && !memcmp(a->hdr,b->hdr,sizeof(a->hdr)) &&
        !memcmp(a->metadata,b->metadata,sizeof(a->metadata));
}

bool rkqms_native_link_clock_uninitialized(const struct rkqms_native_link *l,int error)
{
    u32 bpc,y;u64 expected;
    if(!l || error!=-ESTALE || l->diagnostic!=15 || l->qualified ||
       l->baseline.phy_clock || format(l->baseline.bus_format,&bpc,&y)) return false;
    expected=(u64)l->baseline.mode.clock_khz*1000;
    if(!expected || l->baseline.pixel_clock!=expected) return false;
    expected=expected*bpc/8;if(y==3) expected/=2;
    return expected && l->baseline.tmds_clock==expected;
}

int rkqms_native_link_qualify(struct rkqms_native_link *l)
{
    static u64 cookies; /* Native all-modeset lock serializes this profile. */
    struct rkqms_link_snapshot after;
    u8 fresh[128],override;unsigned int bytes=0,i;
    int ret;
    if (!l) return -EINVAL;
    l->qualified=false;
    ret=rkqms_native_link_observe(l,&l->baseline);if (ret) return ret;
    l->diagnostic=101;
    /* Never adopt someone else's VTEM/VRR or a DSC/CVTEM stream. Only this
     * binding may later enable bit9, after lifecycle ownership is armed. */
    if (l->baseline.scheduler&(BIT(9)|BIT(8)|BIT(10))) return -EBUSY;
    /* Native EDID override cannot certify the physical connected sink. */
    l->diagnostic=102;if (read_at(&override,l->registers->qp+0x600,1) || override) return -EOPNOTSUPP;
    l->diagnostic=103;
    ret=blob_copy(l->drm,l->baseline.edid_blob,l->edid,sizeof(l->edid),&bytes);if (ret) return ret;
    l->diagnostic=104;ret=rkqms_parse_edid(l->edid,bytes,&l->sink);if (ret) return ret;
    /* Use the factory DDC block reader directly. drm_get_edid() updates the
     * live EDID property on this firmware, while drm_do_get_edid() may select
     * firmware overrides. Neither is a physical-only, observation-only API.
     */
    for (i=0;i<bytes/128;++i) {
        l->diagnostic=105+i;
        ret=l->read_edid_block(l->ddc,fresh,i,sizeof(fresh));if (ret) return ret;
        if (memcmp(fresh,l->edid+i*128,128)) return -ESTALE;
    }
    ret=rkqms_native_link_observe(l,&after);if (ret) return ret;
    l->diagnostic=120;
    if (!rkqms_native_link_same(&l->baseline,&after)) return -ESTALE;
    if (cookies==~0ULL) return -EOVERFLOW;
    l->edid_bytes=bytes;l->cookie=++cookies;l->qualified=true;l->diagnostic=0;return 0;
}
int rkqms_native_link_validate(struct rkqms_native_link *l)
{
    struct rkqms_link_snapshot now;
    int ret;
    if (!l || !l->qualified) return -EOPNOTSUPP;
    ret=rkqms_native_link_observe(l,&now);
    if (!ret && !rkqms_native_link_same(&l->baseline,&now)) ret=-ESTALE;
    if (ret) l->qualified=false;
    return ret;
}

int rkqms_native_link_verify_clean(struct rkqms_native_link *l)
{
    struct drm_mode_modeinfo mode;
    struct rkqms_registers *r;
    unsigned long live;u8 power,disabled,active;
    u32 timing,shadow,scheduler;int ret;
    if (!l || !l->registers) return -EINVAL;
    r=l->registers;
    if (!mutex_trylock((void *)(r->qp+0xb48))) return -EBUSY;
    ret=-ESTALE;
    if (r->held || read_at(&power,r->qp+0xb42,1) || read_at(&disabled,r->qp+0xb74,1) ||
        !rkqms_reg_ptr((unsigned long)r->crtc+0x5c0,&live) || read_at(&active,live+9,1)) goto out;
    if (!power || disabled || !active) {
        /* Do not touch powered-down VOP/HDMI registers. An off flag by itself
         * cannot prove owned VTEM was cleared before factory power gating.
         */
        if (!power && disabled && !active && r->lifecycle &&
            r->lifecycle->blocked && !r->lifecycle->owns_vtem &&
            !r->lifecycle->dirty && !r->lifecycle->error) ret=0;
        goto out;
    }
    /* Quiescence certifies removal of QMS, not a usable sink or HDR session.
     * A disconnected or unsupported ordinary native format must still be
     * available to the factory HWC. Full EDID/PHY/AVI/HDR qualification is
     * mandatory separately before constructing another QMS session.
     */
    memset(&mode,0,sizeof(mode));drm_mode_convert_to_umode(&mode,(void *)(live+0x18));
    if (!mode.clock || !mode.vtotal || mode.vtotal>0x1fff ||
        mode.vsync_end<=mode.vsync_start) {ret=-ESTALE;goto out;}
    ret=regmap_read(r->map,0xaa8,&scheduler);if (ret) goto out;
    if (scheduler&BIT(9)) {ret=-EBUSY;goto out;}
    spin_lock((void *)(r->vop+0x28b0));
    timing=readl(r->mmio+r->timing_offset);shadow=READ_ONCE(r->shadow[r->timing_offset/4]);
    spin_unlock((void *)(r->vop+0x28b0));
    ret=timing==shadow && ((timing>>16)&0x1fff)==mode.vtotal &&
        (timing&0xffff)==(u32)(mode.vsync_end-mode.vsync_start) ? 0 : -ESTALE;
out:
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}
