"""Check the real display-policy upgrade guard in confined Android shell files."""
import argparse,hashlib,json,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['adb','serial','work']:p.add_argument('--'+n,required=True)
 p.add_argument('--port',default='5037');a=p.parse_args();w=Path(a.work);w.mkdir(parents=True,exist_ok=False)
 base='/data/local/tmp/rk-display-original-'+hashlib.sha256(str(w.resolve()).encode()).hexdigest()[:10]
 adb=[a.adb,'-P',a.port,'-s',a.serial]
 def shell(s):return subprocess.run(adb+['shell',s],capture_output=True,text=True,encoding='utf-8',timeout=15)
 original=hashlib.sha256(b'original').hexdigest();patched=hashlib.sha256(b'patched').hexdigest()
 s=(ROOT/'magisk-display-policy/original-services.sh').read_text(encoding='utf-8')
 for old,new in [('922c443c3a2b609b7f7f3755bbfbeb90f3c0b0c141829f3b6151371c2aa607b6',original),('/system/framework/services.jar',base+'/mounted.jar'),('/data/adb/modules/rk3588_decode_stack',base+'/previous'),('/proc/sys/kernel/random/boot_id',base+'/boot')]:s=s.replace(old,new)
 f=w/'guard-fixture.sh';f.write_text(s,encoding='utf-8',newline='\n')
 assert shell('test ! -e '+base).returncode==0
 assert shell('mkdir -p '+base+'/next/originals '+base+'/previous/components/display_policy/originals '+base+'/previous/components/display_policy/variants').returncode==0
 subprocess.run(adb+['push',str(f),base+'/next/guard.sh'],capture_output=True,check=True)
 setup='printf original > '+base+'/previous/components/display_policy/originals/services.jar; printf patched > '+base+'/previous/components/display_policy/variants/services.jar; echo '+patched+' > '+base+'/previous/components/display_policy/preflight; echo now > '+base+'/boot; echo now > '+base+'/previous/active-boot-id; echo display_policy=1 > '+base+'/previous/active-config; '
 cases=[]
 for name,extra,ok in [('stock_original','printf original > '+base+'/mounted.jar',True),('recognized_mounted_upgrade','printf patched > '+base+'/mounted.jar',True),('foreign_mounted_rejected','printf foreign > '+base+'/mounted.jar',False),('old_boot_rejected','printf patched > '+base+'/mounted.jar; echo stale > '+base+'/previous/active-boot-id',False),('disabled_rejected','printf patched > '+base+'/mounted.jar; touch '+base+'/previous/disable',False),('wrong_original_rejected','printf patched > '+base+'/mounted.jar; printf wrong > '+base+'/previous/components/display_policy/originals/services.jar',False)]:
  assert shell('rm -f '+base+'/next/originals/services.jar '+base+'/previous/disable').returncode==0
  result=shell(setup+extra+'; sh '+base+'/next/guard.sh');assert (result.returncode==0)==ok,(name,result.stdout,result.stderr)
  cases.append({'case':name,'passed':True})
 (w/'RESULT.json').write_text(json.dumps({'actual_guard_cases':cases,'live_framework_IO':False,'boot_changes':False},indent=2)+'\n',encoding='utf-8')
 print('PASS actual display-policy upgrade guard cases='+str(len(cases))+'; confined files only')
if __name__=='__main__':main()
