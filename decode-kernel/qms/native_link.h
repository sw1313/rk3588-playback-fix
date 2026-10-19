/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_NATIVE_LINK_H
#define RKQMS_NATIVE_LINK_H
/* Exact-Image board observations, process context, native all-modeset locks
 * held. No pointer publication, QMS writes or proof-bit creation here.
 * A qualification reads DDC without updating the connector's EDID property.
 * Per-frame validation compares the live native identity and physical PHY
 * readiness; it never performs slow DDC transactions in the frame window.
 */
#include "native_registers.h"
#include "edid.h"
struct drm_connector;
struct drm_device;
struct drm_property_blob;
struct edid;
struct i2c_adapter;
struct clk;
struct device;
extern struct drm_property_blob *drm_property_lookup_blob(struct drm_device *,u32);
extern void drm_property_blob_put(struct drm_property_blob *);
extern unsigned long clk_get_rate(struct clk *);

#define RKQMS_LINK_EDID_MAX 1024
struct rkqms_link_snapshot {
    struct rkqms_mode mode;
    u64 edid_epoch;
    unsigned long edid_blob;
    u32 connector,crtc,bus_format,encoding,colorspace,bpc,eotf;
    u32 link_config,scheduler,phy_status,interface_status;
    u32 avi[5],hdr[8];
    unsigned long pixel_clock,tmds_clock,phy_clock;
    u8 metadata[32];
};
struct rkqms_native_link {
    struct drm_device *drm;
    struct rkqms_registers *registers;
    unsigned long hdmi,hdptx;
    struct regmap *hpd_map,*phy_grf;
    struct clk *phy_clk;
    struct i2c_adapter *ddc;
    int (*read_edid_block)(void *,u8 *,unsigned int,size_t);
    struct rkqms_link_snapshot baseline;
    struct rkqms_sink sink;
    u64 cookie;
    u8 edid[RKQMS_LINK_EDID_MAX];
    unsigned int edid_bytes;
    int diagnostic; /* Last read-only qualification step; never proof. */
    bool qualified;
};
int rkqms_native_link_prepare(struct rkqms_native_link *,struct drm_device *,
                struct rkqms_registers *,unsigned long hdmi,struct device *phy,
                int (*read_edid_block)(void *,u8 *,unsigned int,size_t));
int rkqms_native_link_observe(struct rkqms_native_link *,struct rkqms_link_snapshot *);
int rkqms_native_link_qualify(struct rkqms_native_link *);
/* Only identifies the observed bootloader-clock-cache case. Not qualification
 * and never permission for QMS register writes. Caller must exclude native
 * clients, verify clean timing and run the factory initialization transaction. */
bool rkqms_native_link_clock_uninitialized(const struct rkqms_native_link *,int);
int rkqms_native_link_validate(struct rkqms_native_link *);
/* Read-only verification of ordinary native timing and disabled VTEM after
 * worker drain/recovery. Does not rearm a stale session or require its old
 * EDID/HDR identity. Powered-off acceptance requires lifecycle cleanup proof.
 */
int rkqms_native_link_verify_clean(struct rkqms_native_link *);
/* Allows the owned VTEM bit to vary, but no native mode/format/HDR change. */
int rkqms_native_link_same(const struct rkqms_link_snapshot *,const struct rkqms_link_snapshot *);
#endif
