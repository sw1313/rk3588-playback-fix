// SPDX-License-Identifier: GPL-2.0-only
/* Actual native_registers/native_lifecycle implementation, synthetic objects,
 * pthread locks and injected MMIO/regmap/native callbacks. No kernel or HDMI.
 */
#include "native_pm.h"
#include <stdio.h>
unsigned int fixture_reads,fixture_writes,fixture_mmio_reads,fixture_mmio_writes;
bool fixture_bad_read,fixture_bad_write;
int fixture_irq;
static struct task_struct task;
static unsigned char qp[0xc78] __attribute__((aligned(64)));
static unsigned char vop[0x2900] __attribute__((aligned(64)));
static unsigned char crtc_object[0x800] __attribute__((aligned(64)));
static unsigned char crtc_state[0x300] __attribute__((aligned(64)));
static u32 mmio[1024],shadow[1024];
static struct regmap map;
static unsigned long original[25];
static struct rkqms_registers registers;
static struct rkqms_native_lifecycle binding;
static unsigned int native_disable_calls,native_enable_calls,native_mode_calls;
static int disabled_with_vtem;
static unsigned char pm_device[0xa0] __attribute__((aligned(64)));
static unsigned char pm_driver[0x100] __attribute__((aligned(64)));
static unsigned long pm_original[23];
static struct rkqms_native_pm pm_binding;
static unsigned int pm_suspends,pm_resumes,quiesces;
static int quiesce_error,resume_error;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static void word(void *p,unsigned int offset,unsigned long value){memcpy((char *)p+offset,&value,sizeof(value));}
static unsigned int finish_sequence=101;
unsigned int drm_crtc_vblank_count(struct drm_crtc *c){(void)c;return finish_sequence;}
void drm_mode_convert_to_umode(struct drm_mode_modeinfo *m,const void *native){memcpy(m,native,sizeof(*m));}
static void factory_disable(struct drm_bridge *b,struct drm_bridge_state *s){
 (void)b;(void)s;++native_disable_calls;disabled_with_vtem=!!(map.words[0xaa8/4]&BIT(9));qp[0xb42]=0;qp[0xb74]=1;
}
static void factory_enable(struct drm_bridge *b,struct drm_bridge_state *s){(void)b;(void)s;++native_enable_calls;qp[0xb42]=1;qp[0xb74]=0;}
static void factory_mode(struct drm_bridge *b,const struct drm_display_mode *m,const struct drm_display_mode *a){(void)b;(void)m;(void)a;++native_mode_calls;}
static int factory_suspend(struct device *d){(void)d;++pm_suspends;return 0;}
static int factory_resume(struct device *d){(void)d;++pm_resumes;return resume_error;}
static int native_quiesce(void *p){(void)p;++quiesces;return quiesce_error;}
static void setup(void){
 fixture_current=&task;
 mutex_init((void *)(qp+0xb48));spin_lock_init((spinlock_t *)(vop+0x28b0));
 registers.crtc=(void *)crtc_object;registers.qp=(unsigned long)qp;registers.vop=(unsigned long)vop;
 registers.map=&map;registers.mmio=mmio;registers.shadow=shadow;registers.timing_offset=0xc50;
 registers.control_offset=0xc00;registers.vact_offset=0xc54;registers.status_offset=0x60;
 registers.base.vtotal=2250;registers.base.vsync_start=2168;registers.base.vsync_end=2178;
 registers.flushed=1;registers.task=current;registers.sequence=100;registers.expected_timing=(2250U<<16)|10;
 mmio[0xc50/4]=shadow[0xc50/4]=registers.expected_timing;
 qp[0xb42]=1;qp[0xb74]=0;crtc_state[9]=1;word(crtc_object,0x5c0,(unsigned long)crtc_state);
 original[6]=(unsigned long)factory_mode;original[10]=(unsigned long)factory_enable;original[11]=(unsigned long)factory_disable;
 word(qp,0x7b8,(unsigned long)original);word(qp,0x7c0,(unsigned long)qp);
}
int main(void){
 unsigned int cases=0;struct drm_bridge *bridge=(void *)(qp+0x720);
 setup();CHECK(!rkqms_registers_finish(&registers));CHECK(fixture_mmio_reads==1);++cases;
 for(unsigned int i=0;i<2;++i){
  finish_sequence=i?102:100;fixture_mmio_reads=0;
  CHECK(rkqms_registers_finish(&registers)==-EIO && registers.diagnostic==81 &&
    registers.completion_sequence==finish_sequence && !fixture_mmio_reads);++cases;
 }
 finish_sequence=101;mmio[0xc50/4]^=1;fixture_mmio_reads=0;
 CHECK(rkqms_registers_finish(&registers)==-EIO && registers.diagnostic==84 &&
   registers.completion_timing==mmio[0xc50/4]);mmio[0xc50/4]^=1;++cases;
 fixture_mmio_reads=0;qp[0xb42]=0;CHECK(rkqms_registers_finish(&registers)<0 && !fixture_mmio_reads);++cases;
 qp[0xb42]=1;qp[0xb74]=1;CHECK(rkqms_registers_finish(&registers)<0 && !fixture_mmio_reads);++cases;
 qp[0xb74]=0;crtc_state[9]=0;CHECK(rkqms_registers_finish(&registers)<0 && !fixture_mmio_reads);++cases;
 crtc_state[9]=1;registers.expected_vtem=1;
 CHECK(rkqms_registers_finish(&registers)<0);registers.expected_vtem=0;++cases;
 map.words[0xaa8/4]=BIT(9)|BIT(10)|BIT(13)|BIT(2);
 CHECK(rkqms_registers_disable_owned_vtem(&registers,false)==-EBUSY && !map.writes);++cases;
 CHECK(!rkqms_registers_disable_owned_vtem(&registers,true));
 CHECK(map.words[0xaa8/4]==(BIT(10)|BIT(13)|BIT(2)));++cases;
 CHECK(!rkqms_native_lifecycle_prepare(&binding,&registers,(unsigned long)original,
       (unsigned long)factory_mode,(unsigned long)factory_enable,(unsigned long)factory_disable));++cases;
 CHECK(registers.lifecycle==&binding.state && binding.state.blocked);++cases;
 CHECK(!rkqms_lifecycle_arm(&binding.state,1));registers.expected_epoch=1;
 rkqms_lifecycle_claim_vtem(&binding.state);map.words[0xaa8/4]|=BIT(9);
 {
  struct rkqms_state c={0};unsigned long commit_owner=(unsigned long)current;
  unsigned int writes=map.writes,mmio_writes=fixture_mmio_writes;
  c.vic=97;c.current_tfr=c.packet_tfr=2;c.current_vtotal=5625;
  c.base_tfr=10;c.vtem_enabled=c.packet_const=1;c.base.height=2160;c.base.vsync_start=2168;
  registers.commit_mutex=(unsigned long)&commit_owner;registers.expected_vtem=1;
  registers.scheduler_before=map.words[0xaa8/4];registers.expected_timing=(5625U<<16)|10;
  mmio[0xc50/4]=shadow[0xc50/4]=registers.expected_timing;
  map.words[0xd00/4]=0xc000;map.words[0xd04/4]=0xc6;map.words[0xd08/4]=0x06040000;
  map.words[0xd0c/4]=(60U<<16)|(2U<<11)|8;
  CHECK(!rkqms_registers_hold(&registers,&c));++cases;
  map.words[0xd04/4]=0x86;
  CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&registers.diagnostic==104);++cases;
  map.words[0xd04/4]=0xc6;
  /* An integer notice may advertise the NEXT rate while repeating the
   * previous timing. Verify the exact variable packet; do not accept the
   * constant packet or silently replace the advertised target. */
  c.packet_const=0;c.packet_tfr=3;
  CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE);++cases;
  CHECK(rkqms_registers_repeat(&registers,&c)==-ESTALE);++cases;
  map.words[0xd08/4]=0x04040000;map.words[0xd0c/4]=(60U<<16)|(3U<<11)|8;
  mmio[0xc00/4]=0x20;mmio[0xc54/4]=(82U<<16)|2242;mmio[0x60/4]=225U<<16;
  CHECK(!rkqms_registers_repeat(&registers,&c));++cases;
  map.words[0xd0c/4]^=(1U<<11);
  CHECK(rkqms_registers_repeat(&registers,&c)==-ESTALE&&registers.diagnostic==104);
  map.words[0xd0c/4]^=(1U<<11);++cases;
  c.current_tfr=1;CHECK(rkqms_registers_repeat(&registers,&c)==-ESTALE);++cases;
  c.current_tfr=c.packet_tfr=2;c.packet_const=1;
  map.words[0xd08/4]=0x06040000;map.words[0xd0c/4]=(60U<<16)|(2U<<11)|8;
  map.words[0xd04/4]^=1;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&registers.diagnostic==104);map.words[0xd04/4]^=1;++cases;
  /* Between transactions native audio/HDR owners may change their packet
   * bits. Our read-only hold must neither undo them nor force a modeset. */
  map.words[0xaa8/4]^=BIT(2);CHECK(!rkqms_registers_hold(&registers,&c));
  CHECK(registers.observed_scheduler==map.words[0xaa8/4]);map.words[0xaa8/4]^=BIT(2);++cases;
  map.words[0xaa8/4]^=BIT(9);
  CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&registers.diagnostic==103);
  map.words[0xaa8/4]^=BIT(9);++cases;
  /* During an actual submitted commit the original preservation check is
   * retained: another packet-bit change still invalidates publication. */
  map.words[0xaa8/4]^=BIT(2);registers.sequence=100;
  CHECK(rkqms_registers_finish(&registers)==-ESTALE&&registers.diagnostic==85);
  map.words[0xaa8/4]^=BIT(2);++cases;
  mmio[0xc50/4]^=1;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&registers.diagnostic==105);mmio[0xc50/4]^=1;++cases;
  shadow[0xc50/4]^=1;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE);shadow[0xc50/4]^=1;++cases;
  fixture_mmio_reads=0;qp[0xb42]=0;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&!fixture_mmio_reads);qp[0xb42]=1;++cases;
  fixture_mmio_reads=0;binding.state.blocked=1;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&!fixture_mmio_reads);binding.state.blocked=0;++cases;
  binding.state.owns_vtem=0;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE);binding.state.owns_vtem=1;++cases;
  commit_owner=0;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&registers.diagnostic==100);commit_owner=(unsigned long)current;++cases;
  c.current_tfr=c.packet_tfr=1;CHECK(rkqms_registers_hold(&registers,&c)==-ESTALE&&registers.diagnostic==100);++cases;
  CHECK(map.writes==writes&&fixture_mmio_writes==mmio_writes);++cases;
  registers.expected_vtem=0;registers.expected_timing=(2250U<<16)|10;
  mmio[0xc50/4]=shadow[0xc50/4]=registers.expected_timing;
 }
 ((void (*)(struct drm_bridge *,struct drm_bridge_state *))binding.table[11])(bridge,NULL);
 CHECK(native_disable_calls==1 && !disabled_with_vtem && binding.state.blocked && !binding.state.owns_vtem);++cases;
 CHECK(rkqms_lifecycle_check(&binding.state,1)==RKQMS_ESTALE);++cases;
 /* Power already off: retain ownership/error, then retry after factory enable. */
 rkqms_lifecycle_claim_vtem(&binding.state);map.words[0xaa8/4]|=BIT(9);
 ((void (*)(struct drm_bridge *,struct drm_bridge_state *))binding.table[10])(bridge,NULL);
 CHECK(native_enable_calls==1 && !binding.state.owns_vtem && !binding.state.error && binding.state.blocked);++cases;
 CHECK(!(map.words[0xaa8/4]&BIT(9)));++cases;
 ((void (*)(struct drm_bridge *,const struct drm_display_mode *,const struct drm_display_mode *))binding.table[6])(bridge,NULL,NULL);
 CHECK(native_mode_calls==1 && binding.state.blocked);++cases;
 rkqms_lifecycle_claim_vtem(&binding.state);map.words[0xaa8/4]|=BIT(9);map.write_error=-EIO;
 ((void (*)(struct drm_bridge *,struct drm_bridge_state *))binding.table[11])(bridge,NULL);
 CHECK(binding.state.dirty && binding.state.owns_vtem && binding.state.error==-EIO && native_disable_calls==2);++cases;
 map.write_error=0;
 ((void (*)(struct drm_bridge *,struct drm_bridge_state *))binding.table[10])(bridge,NULL);
 CHECK(!binding.state.dirty && !binding.state.owns_vtem && binding.state.blocked);++cases;
 fixture_mmio_reads=0;registers.expected_epoch=binding.state.epoch;
 CHECK(rkqms_registers_finish(&registers)<0 && !fixture_mmio_reads);++cases;
 word(pm_device,0x68,(unsigned long)pm_driver);word(pm_driver,0x78,(unsigned long)pm_original);
 for(unsigned int i=2;i<=7;++i)pm_original[i]=(unsigned long)(i&1?factory_resume:factory_suspend);
 CHECK(!rkqms_native_pm_prepare(&pm_binding,&binding,(void *)pm_device,(unsigned long)pm_original,
       (unsigned long)factory_suspend,(unsigned long)factory_resume,native_quiesce,NULL));++cases;
 quiesce_error=-ETIMEDOUT;
 CHECK(((int (*)(struct device *))pm_binding.table[2])((void *)pm_device)==-ETIMEDOUT && !pm_suspends);++cases;
 quiesce_error=0;
 CHECK(!((int (*)(struct device *))pm_binding.table[2])((void *)pm_device) && pm_suspends==1 && quiesces==2);++cases;
 {rq_u64 epoch=binding.state.epoch;
 CHECK(!((int (*)(struct device *))pm_binding.table[4])((void *)qp) && pm_suspends==2 && binding.state.epoch==epoch);++cases;}
 CHECK(!((int (*)(struct device *))pm_binding.table[3])((void *)pm_device) && pm_resumes==1 && binding.state.blocked);++cases;
 resume_error=-ENODEV;
 CHECK(((int (*)(struct device *))pm_binding.table[7])((void *)pm_device)==-ENODEV && binding.state.error==-ENODEV && binding.state.dirty);++cases;
 CHECK(!rkqms_native_pm_discard_unpublished(&pm_binding));++cases;
 pm_original[20]=1;
 CHECK(rkqms_native_pm_prepare(&pm_binding,&binding,(void *)pm_device,(unsigned long)pm_original,
       (unsigned long)factory_suspend,(unsigned long)factory_resume,native_quiesce,NULL)==-ESTALE);++cases;
 CHECK(!rkqms_native_lifecycle_discard_unpublished(&binding) && !registers.lifecycle);++cases;
 /* Boundary evidence is read-only and reports sampling failures separately.
  * It does not manufacture hardware acceptance or change transaction state. */
 {
  struct rkqms_boundary_sample sample;
  unsigned int writes=map.writes,reads=map.reads;
  map.words[0xaac/4]=1;map.words[0x8e0/4]=1;map.words[0x8e8/4]=0x40;
  map.words[0xd08/4]=0x04040000;map.words[0xd0c/4]=(60U<<16)|(3U<<11)|8;
  rkqms_registers_sample(&registers,&sample);
  CHECK(!sample.error && sample.gcp_control==1 && sample.hdcp2_config==1 &&
        sample.hdcp2_gpio==0x40 && sample.packet_mode==0x04040000 &&
        sample.packet_rate==map.words[0xd0c/4] && sample.dsp_control==0x20 &&
        sample.vact==((82U<<16)|2242) && sample.vcnt==225 &&
        map.writes==writes && map.reads==reads+5);++cases;
  map.read_error=-EIO;reads=map.reads;
  rkqms_registers_sample(&registers,&sample);
  CHECK(sample.error==-EIO && !sample.packet_rate && map.writes==writes && map.reads==reads+1);++cases;
  map.read_error=0;
  registers.held=1;registers.capture_boundary=0;
  memset(&registers.mode_write,0,sizeof(registers.mode_write));
  CHECK(!rkqms_registers_packet(&registers,0xd08,0x06040000) && !registers.mode_write.ns);++cases;
  registers.capture_boundary=1;
  CHECK(!rkqms_registers_packet(&registers,0xd08,0x04040000));
  CHECK(registers.mode_write.ns && registers.mode_write.vcnt==225 &&
        registers.mode_write.sequence==finish_sequence);++cases;
  CHECK(!rkqms_registers_packet(&registers,0xd0c,(60U<<16)|(2U<<11)|8));
  CHECK(registers.rate_write.ns>=registers.mode_write.ns && registers.rate_write.vcnt==225);++cases;
  {u64 previous=registers.rate_write.ns;map.write_error=-EIO;
   CHECK(rkqms_registers_packet(&registers,0xd0c,0)==-EIO && registers.rate_write.ns==previous);
   map.write_error=0;++cases;}
  registers.held=0;registers.capture_boundary=0;
 }
 printf("native_lifecycle_cases=%u; native_objects_and_IO=injected; display_IO=0\n",cases);return 0;
}
