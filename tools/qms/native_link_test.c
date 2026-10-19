// SPDX-License-Identifier: GPL-2.0-only
/* Actual board reader/qualifier with injected native objects and I/O. No
 * display hardware, live DDC, endpoint, callback publication or proof bits. */
#include "native_binding.h"
#include <stdio.h>
unsigned int fixture_reads,fixture_writes,fixture_mmio_reads,fixture_mmio_writes;
bool fixture_bad_read,fixture_bad_write;int fixture_irq;
static u8 qp[0xc78] __attribute__((aligned(64))),crtc[0x800],cs[0x300],conn[0x100];
static u8 hdmi[0x300],phy_device[0x100],phy[0x100],generic[0x100];
static u8 vop[0x2900] __attribute__((aligned(64)));
static u32 mmio[1024],shadow[1024];
static struct rkqms_lifecycle lifecycle;
static u8 eb[0x58],hb[0x58],edid[256],wire[256],metadata[32];
static struct regmap map,hpd,phy_grf,phy_map;
static struct rkqms_registers regs;
static struct rkqms_native_link test_link;
static unsigned int refs,ddc_calls;static int ddc_error,ddc_change_epoch;
static unsigned long clock_rate=297000000;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static void word(void *p,unsigned int off,unsigned long value){memcpy((u8 *)p+off,&value,8);}
static void integer(void *p,unsigned int off,u32 value){memcpy((u8 *)p+off,&value,4);}
unsigned long clk_get_rate(struct clk *c){(void)c;return clock_rate;}
unsigned int drm_crtc_vblank_count(struct drm_crtc *c){(void)c;return 1;}
void drm_mode_convert_to_umode(struct drm_mode_modeinfo *m,const void *p){memcpy(m,p,sizeof(*m));}
struct drm_property_blob *drm_property_lookup_blob(struct drm_device *d,u32 id){
 (void)d;if(id==10){++refs;return (void *)eb;}if(id==11){++refs;return (void *)hb;}return NULL;
}
void drm_property_blob_put(struct drm_property_blob *b){(void)b;--refs;}
static int ddc(void *p,u8 *out,unsigned int block,size_t bytes){
 (void)p;++ddc_calls;if(ddc_error)return ddc_error;
 if(block>1 || bytes!=128)return -EINVAL;
 memcpy(out,wire+block*128,128);if(ddc_change_epoch)word(qp,0x608,2);return 0;
}
static void checksum(u8 *p){unsigned int i,sum=0;for(i=0;i<127;++i)sum+=p[i];p[127]=-sum;}
static void packet(u32 offset,u32 type,u32 version,u32 length,u8 *data){
 unsigned int i,sum=type+version+length;u8 payload[28]={0};
 for(i=0;i<length;++i){payload[1+i]=data[i];sum+=data[i];}payload[0]=-sum;
 map.words[offset/4]=(length<<16)|(version<<8);
 for(i=0;i<=length;++i){u32 *v=&map.words[offset/4+1+i/4];if(!(i%4))*v=0;*v|=(u32)payload[i]<<(8*(i%4));}
}
static void setup(void){
 struct drm_mode_modeinfo m={0};u8 avi[13]={0x60,0x80,0,97};
 mutex_init((void *)(qp+0xb48));
 regs.qp=(unsigned long)qp;regs.crtc=(void *)crtc;regs.map=&map;
 regs.vop=(unsigned long)vop;regs.mmio=mmio;regs.shadow=shadow;regs.timing_offset=0xc50;
 spin_lock_init((spinlock_t *)(vop+0x28b0));
 mmio[0xc50/4]=shadow[0xc50/4]=(2250U<<16)|10;
 qp[0xb42]=1;qp[0xb40]=1;cs[9]=1;
 word(crtc,0x5c0,(unsigned long)cs);integer(crtc,0x60,72);
 word(qp,0x6d8,(unsigned long)conn);word(conn,8,(unsigned long)crtc);
 word(qp,0x160,(unsigned long)eb);integer(qp,0x28,80);word(qp,0x608,1);
 integer(eb,0,10);word(eb,0x48,256);word(eb,0x50,(unsigned long)edid);
 integer(hb,0,11);word(hb,0x48,32);word(hb,0x50,(unsigned long)metadata);
 integer(qp,0x83c,0x2026);integer(qp,0x844,2);word(qp,0x858,594000000);integer(qp,0x870,297000000);
 word(hdmi,0x100,(unsigned long)qp);word(hdmi,0x90,0x12340000);word(hdmi,0x128,0x2026);
 word(phy_device,0x78,(unsigned long)phy);word(phy,0,(unsigned long)phy_device);
 word(phy,8,(unsigned long)&phy_map);phy_map.device=(void *)phy_device;
 word(phy,0x20,(unsigned long)generic);word(generic,0x78,(unsigned long)phy);
 word(hdmi,0x108,(unsigned long)generic);
 word(hdmi,8,(unsigned long)&hpd);word(phy,0x10,(unsigned long)&phy_grf);
 word(phy,0x58,0x1234);word(qp,0xb30,0x2345);
 hpd.words[0x384/4]=BIT(16);phy_grf.words[0x80/4]=0xa;map.words[0xaa8/4]=BIT(13)|BIT(3);
 m.clock=594000;m.flags=5;m.hdisplay=3840;m.vdisplay=2160;m.htotal=4400;m.vtotal=2250;
 m.vsync_start=2168;m.vsync_end=2178;memcpy(cs+0x18,&m,sizeof(m));
 edid[0]=edid[7]=0;memset(edid+1,255,6);edid[126]=1;checksum(edid);
 edid[128]=2;edid[129]=3;edid[130]=16;edid[132]=(3U<<5)|11;
 edid[133]=0xd8;edid[134]=0x5d;edid[135]=0xc4;edid[136]=1;
 edid[140]=0x40;edid[141]=40;edid[142]=120;edid[143]=0x30;checksum(edid+128);memcpy(wire,edid,256);
 packet(0xbe0,0x82,2,13,avi);
}
int main(void){
 unsigned int cases=0,before;struct rkqms_link_snapshot now;
 setup();CHECK(!rkqms_native_link_prepare(&test_link,(void *)0x12340000,&regs,(unsigned long)hdmi,(void *)phy_device,ddc));++cases;
 word(hdmi,0x108,0);CHECK(rkqms_native_link_prepare(&test_link,(void *)0x12340000,&regs,(unsigned long)hdmi,(void *)phy_device,ddc)==-ESTALE);word(hdmi,0x108,(unsigned long)generic);++cases;
 CHECK(!rkqms_native_link_qualify(&test_link) && test_link.qualified && ddc_calls==2 && !refs);++cases;
 {
  struct rkqms_native_lifecycle life={0};struct rkqms_native_binding b={0};struct rkqms_session s={0};
  rkqms_lifecycle_init(&life.state);CHECK(!rkqms_lifecycle_arm(&life.state,1));
  b.life=&life;b.link=&test_link;b.registers=&regs;regs.expected_epoch=1;regs.generation=1;
  regs.base=test_link.baseline.mode;s.transaction.core.base=regs.base;
  s.status.generation=1;s.status.link_cookie=test_link.cookie;s.status.connector=test_link.baseline.connector;
  s.status.crtc=test_link.baseline.crtc;s.status.bits_per_component=8;s.status.eotf=0;
  s.status.colorspace=test_link.baseline.colorspace;s.status.width=3840;s.status.height=2160;
  s.status.tfr_mask=test_link.sink.tfr_mask;
  CHECK(!rkqms_binding_validate(&b,&s));++cases;
  life.pm_blocked=true;CHECK(rkqms_binding_validate(&b,&s)==-EAGAIN);life.pm_blocked=false;++cases;
  ++s.status.link_cookie;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.link_cookie;++cases;
  ++s.status.generation;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.generation;++cases;
  ++s.status.bits_per_component;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.bits_per_component;++cases;
  ++s.status.eotf;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.eotf;++cases;
  ++s.status.colorspace;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.colorspace;++cases;
  ++s.status.connector;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.connector;++cases;
  ++s.status.crtc;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.status.crtc;++cases;
  s.status.tfr_mask|=BIT(30);CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);s.status.tfr_mask=test_link.sink.tfr_mask;++cases;
  ++s.transaction.core.base.vtotal;CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);--s.transaction.core.base.vtotal;++cases;
  rkqms_lifecycle_invalidate(&life.state,RKQMS_LIFE_MODE);CHECK(rkqms_binding_validate(&b,&s)==-ESTALE);++cases;
 }
 CHECK(test_link.cookie);++cases;
 for(unsigned int bit=8;bit<=10;++bit){map.words[0xaa8/4]|=BIT(bit);CHECK(rkqms_native_link_qualify(&test_link)==-EBUSY && !test_link.qualified);map.words[0xaa8/4]&=~BIT(bit);++cases;}
 CHECK(!rkqms_native_link_qualify(&test_link));
 CHECK(!rkqms_native_link_verify_clean(&test_link));++cases;
 map.words[0xaa8/4]|=BIT(9);CHECK(rkqms_native_link_verify_clean(&test_link)==-EBUSY);map.words[0xaa8/4]&=~BIT(9);++cases;
 mmio[0xc50/4]=(2251U<<16)|10;CHECK(rkqms_native_link_verify_clean(&test_link)<0);mmio[0xc50/4]=shadow[0xc50/4];++cases;
 /* OFF alone must not certify cleanup of a formerly owned VTEM bit. */
 qp[0xb42]=0;qp[0xb74]=1;cs[9]=0;before=fixture_mmio_reads;
 CHECK(rkqms_native_link_verify_clean(&test_link)<0 && before==fixture_mmio_reads);++cases;
 regs.lifecycle=&lifecycle;lifecycle.blocked=1;
 CHECK(!rkqms_native_link_verify_clean(&test_link) && before==fixture_mmio_reads);++cases;
 lifecycle.owns_vtem=1;CHECK(rkqms_native_link_verify_clean(&test_link)<0 && before==fixture_mmio_reads);lifecycle.owns_vtem=0;++cases;
 lifecycle.dirty=1;CHECK(rkqms_native_link_verify_clean(&test_link)<0);lifecycle.dirty=0;++cases;
 lifecycle.error=-EIO;CHECK(rkqms_native_link_verify_clean(&test_link)<0);lifecycle.error=0;++cases;
 qp[0xb42]=1;qp[0xb74]=0;cs[9]=1;
 before=ddc_calls;CHECK(!rkqms_native_link_validate(&test_link) && before==ddc_calls);++cases;
 qp[0xb42]=0;before=map.reads;CHECK(rkqms_native_link_observe(&test_link,&now)<0 && map.reads==before);qp[0xb42]=1;++cases;
 qp[0xb74]=1;CHECK(rkqms_native_link_observe(&test_link,&now)<0 && map.reads==before);qp[0xb74]=0;++cases;
 cs[9]=0;CHECK(rkqms_native_link_observe(&test_link,&now)<0 && map.reads==before);cs[9]=1;++cases;
 hpd.words[0x384/4]=0;CHECK(rkqms_native_link_observe(&test_link,&now)==-ENOLINK);hpd.words[0x384/4]=BIT(16);++cases;
 phy_grf.words[0x80/4]=8;CHECK(rkqms_native_link_observe(&test_link,&now)==-ENOLINK);phy_grf.words[0x80/4]=0xa;++cases;
 phy_grf.read_error=-EIO;CHECK(rkqms_native_link_observe(&test_link,&now)==-EIO);phy_grf.read_error=0;++cases;
 map.words[0x968/4]=1;CHECK(rkqms_native_link_observe(&test_link,&now)==-EOPNOTSUPP);map.words[0x968/4]=0;++cases;
 map.words[0x968/4]=16;CHECK(rkqms_native_link_observe(&test_link,&now)==-EOPNOTSUPP);map.words[0x968/4]=0;++cases;
 integer(qp,0x83c,0x2027);CHECK(rkqms_native_link_observe(&test_link,&now)<0);integer(qp,0x83c,0x2026);++cases;
 clock_rate=296000000;CHECK(rkqms_native_link_observe(&test_link,&now)<0);clock_rate=297000000;++cases;
 /* An uninitialized boot clock is a reason for factory initialization,
  * never a qualified link or a way to accept other clock mismatches. */
 clock_rate=0;CHECK(rkqms_native_link_qualify(&test_link)==-ESTALE &&
   rkqms_native_link_clock_uninitialized(&test_link,-ESTALE) && !test_link.qualified);++cases;
 CHECK(!rkqms_native_link_clock_uninitialized(NULL,-ESTALE));++cases;
 CHECK(!rkqms_native_link_clock_uninitialized(&test_link,-EIO));++cases;
 test_link.diagnostic=14;CHECK(!rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));test_link.diagnostic=15;++cases;
 test_link.qualified=true;CHECK(!rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));test_link.qualified=false;++cases;
 ++test_link.baseline.pixel_clock;CHECK(!rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));--test_link.baseline.pixel_clock;++cases;
 ++test_link.baseline.tmds_clock;CHECK(!rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));--test_link.baseline.tmds_clock;++cases;
 test_link.baseline.phy_clock=296000000;CHECK(!rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));++cases;
 clock_rate=296000000;CHECK(rkqms_native_link_qualify(&test_link)==-ESTALE &&
   !rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));++cases;
 clock_rate=297000000;CHECK(!rkqms_native_link_qualify(&test_link) &&
   !rkqms_native_link_clock_uninitialized(&test_link,-ESTALE));++cases;
 map.words[0xbe4/4]^=1;CHECK(rkqms_native_link_observe(&test_link,&now)<0);map.words[0xbe4/4]^=1;++cases;
 map.words[0xaa8/4]&=~BIT(13);CHECK(rkqms_native_link_observe(&test_link,&now)<0);map.words[0xaa8/4]|=BIT(13);++cases;
 qp[0x600]=1;CHECK(rkqms_native_link_qualify(&test_link)==-EOPNOTSUPP && !test_link.qualified);qp[0x600]=0;++cases;
 ddc_error=-EIO;CHECK(rkqms_native_link_qualify(&test_link)==-EIO && !test_link.qualified);ddc_error=0;++cases;
 wire[1]^=1;CHECK(rkqms_native_link_qualify(&test_link)==-ESTALE);wire[1]^=1;++cases;
 ddc_change_epoch=1;CHECK(rkqms_native_link_qualify(&test_link)==-ESTALE);ddc_change_epoch=0;word(qp,0x608,1);++cases;
 CHECK(!rkqms_native_link_qualify(&test_link));word(qp,0x608,2);CHECK(rkqms_native_link_validate(&test_link)==-ESTALE && !test_link.qualified);word(qp,0x608,1);++cases;
 CHECK(!rkqms_native_link_qualify(&test_link));map.words[0xaa8/4]|=BIT(9);CHECK(!rkqms_native_link_validate(&test_link));map.words[0xaa8/4]&=~BIT(9);++cases;
 word(eb,0x48,2048);CHECK(rkqms_native_link_qualify(&test_link)<0 && !refs);word(eb,0x48,256);++cases;
 integer(eb,0,99);CHECK(rkqms_native_link_qualify(&test_link)<0 && !refs);integer(eb,0,10);++cases;
 /* Genuine ten-bit HDR fixture, never force SDR to qualify. */
 integer(qp,0x83c,0x2027);word(hdmi,0x128,0x2027);integer(qp,0x870,371250000);clock_rate=371250000;
 word(conn,0x90,(unsigned long)hb);metadata[4]=2;metadata[6]=0x21;metadata[8]=0x43;
 packet(0xc60,0x87,1,26,metadata+4);map.words[0xaa8/4]|=BIT(17);
 CHECK(!rkqms_native_link_qualify(&test_link) && test_link.baseline.bpc==10 && test_link.baseline.eotf==2);++cases;
 map.words[0xc64/4]^=1;CHECK(rkqms_native_link_observe(&test_link,&now)<0);map.words[0xc64/4]^=1;++cases;
 metadata[6]^=1;CHECK(rkqms_native_link_observe(&test_link,&now)<0);metadata[6]^=1;++cases;
 map.words[0xaa8/4]&=~BIT(17);CHECK(rkqms_native_link_observe(&test_link,&now)<0);map.words[0xaa8/4]|=BIT(17);++cases;
 word(conn,0x90,0);CHECK(rkqms_native_link_observe(&test_link,&now)<0);word(conn,0x90,(unsigned long)hb);++cases;
 integer(qp,0x83c,0x2026);word(hdmi,0x128,0x2026);integer(qp,0x870,297000000);clock_rate=297000000;
 CHECK(rkqms_native_link_observe(&test_link,&now)==-EOPNOTSUPP);++cases;
 CHECK(!refs && !map.writes && !phy_grf.writes && !hpd.writes && !fixture_mmio_writes);++cases;
 printf("{\"native_link_cases\":%u,\"display_IO\":false,\"DDC\":\"injected\",\"proof_bits_created\":false}\n",cases);return 0;
}
