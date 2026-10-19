/* SPDX-License-Identifier: GPL-2.0-only */
#include "core.c"
#include "edid.c"
#include "native_link.c"
extern void drm_modeset_lock_all(struct drm_device *);
extern void drm_modeset_unlock_all(struct drm_device *);
static int link_result=-EAGAIN,link_qualified,link_qms,link_tfr_mask,link_edid_bytes;
static int link_native_clean;
static int link_vrr_min,link_vrr_max,link_bpc,link_eotf,link_format,link_phy_status;
static unsigned long link_phy_clock,link_tmds_clock;
module_param(link_result,int,0444);
module_param(link_native_clean,int,0444);
module_param(link_qualified,int,0444);
module_param(link_qms,int,0444);
module_param(link_tfr_mask,int,0444);
module_param(link_edid_bytes,int,0444);
module_param(link_vrr_min,int,0444);
module_param(link_vrr_max,int,0444);
module_param(link_bpc,int,0444);
module_param(link_eotf,int,0444);
module_param(link_format,int,0444);
module_param(link_phy_status,int,0444);
module_param(link_phy_clock,ulong,0444);
module_param(link_tmds_clock,ulong,0444);
static struct rkqms_native_link observed_link;
static struct rkqms_registers observed_registers;
static int inspect_live_link(unsigned long drm,unsigned long hdmi,unsigned long qp,
                              unsigned long vop,struct device *phy)
{
    static const long tables[]={DISPLAY_VP_REGS0_DELTA,DISPLAY_VP_REGS1_DELTA,
                               DISPLAY_VP_REGS2_DELTA,DISPLAY_VP_REGS3_DELTA};
    struct drm_crtc *crtc;
    struct drm_mode_modeinfo mode;
    struct rkqms_mode base={0};
    unsigned long cs,owner;u8 active,port;unsigned int i;int ret=-ENODEV;
    drm_modeset_lock_all((void *)drm);
    for (i=0;i<4;++i) {
        crtc=drm_crtc_from_index((void *)drm,i);
        if (!crtc || !rkqms_reg_ptr((unsigned long)crtc+0x5c0,&cs) ||
            copy_from_kernel_nofault(&active,(void *)(cs+9),1)) {ret=-ESTALE;break;}
        if (!active) continue;
        if (!rkqms_reg_ptr(qp+0x6d8,&cs) ||
            !rkqms_reg_ptr(cs+8,&owner) || owner!=(unsigned long)crtc ||
            !rkqms_reg_ptr((unsigned long)crtc+0x5c0,&cs) ||
            copy_from_kernel_nofault(&port,(void *)((unsigned long)crtc+0x688),1) || port>3) {ret=-ESTALE;break;}
        memset(&mode,0,sizeof(mode));drm_mode_convert_to_umode(&mode,(void *)(cs+0x18));
        base.clock_khz=mode.clock;base.flags=mode.flags;base.width=mode.hdisplay;base.height=mode.vdisplay;
        base.htotal=mode.htotal;base.vtotal=mode.vtotal;base.vsync_start=mode.vsync_start;base.vsync_end=mode.vsync_end;
        ret=rkqms_registers_bind(&observed_registers,crtc,qp,vop,anchor+DISPLAY_VOP_DATA_DELTA,anchor+tables[port],&base);
        if (!ret) ret=rkqms_native_link_prepare(&observed_link,(void *)drm,&observed_registers,hdmi,phy,
                             (void *)(anchor+DISPLAY_DDC_BLOCK_DELTA));
        if (!ret) ret=rkqms_native_link_qualify(&observed_link);
        if (!ret) ret=rkqms_native_link_validate(&observed_link);
        if (!ret) {ret=rkqms_native_link_verify_clean(&observed_link);link_native_clean=!ret;}
        break;
    }
    drm_modeset_unlock_all((void *)drm);
    link_result=ret;link_qualified=!ret && observed_link.qualified;
    link_qms=observed_link.sink.qms;link_tfr_mask=observed_link.sink.tfr_mask;
    link_vrr_min=observed_link.sink.vrr_min;link_vrr_max=observed_link.sink.vrr_max;
    link_edid_bytes=observed_link.edid_bytes;link_bpc=observed_link.baseline.bpc;
    link_eotf=observed_link.baseline.eotf;link_format=observed_link.baseline.bus_format;
    link_phy_status=observed_link.baseline.phy_status;link_phy_clock=observed_link.baseline.phy_clock;
    link_tmds_clock=observed_link.baseline.tmds_clock;
    pr_info("RK_QMS_LINK: result=%d qualified=%d qms=%d edid=%d format=%x bpc=%d eotf=%d phy=%x phy_clk=%lu tmds=%lu; no commit, display writes, hooks or QMS activation\n",
            ret,link_qualified,link_qms,link_edid_bytes,link_format,link_bpc,link_eotf,link_phy_status,link_phy_clock,link_tmds_clock);
    return ret;
}
