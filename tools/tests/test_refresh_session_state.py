"""Exercise independent session rollback with confined Android files; no live modules."""
import argparse,hashlib,json,shlex,subprocess,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['adb','serial','work']:p.add_argument('--'+name,required=True)
 p.add_argument('--port',default='5037');a=p.parse_args();w=Path(a.work);w.mkdir(parents=True,exist_ok=False)
 base='/data/local/tmp/rk-session-state-'+hashlib.sha256(str(w.resolve()).encode()).hexdigest()[:8]
 adb=[a.adb,'-P',a.port,'-s',a.serial]
 def shell(s):return subprocess.run(adb+['shell',s],capture_output=True,text=True,encoding='utf-8',timeout=15)
 folder=w/'fixture';folder.mkdir()
 def put(path,text):
  f=folder/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(text,encoding='utf-8',newline='\n')
 source=(ROOT/'magisk-decode/refresh-session-state.sh').read_text()
 source=source.replace('/data/adb/rk3588-refresh-session',base+'/state').replace('/proc/sys/kernel/random/boot_id',base+'/boot').replace('"/proc/$pid/maps"','"'+base+'/maps"')
 stub='resetprop() { printf "%s\\n" "$*" >> '+base+'/property-writes; }\ngetprop() { cat '+base+'/runtime; }\npidof() { echo 123; }\n'
 source=source.replace('set -eu','set -eu\n'+stub)
 put('module/state.sh',source);put('module/config','display_session=1\ndisplay_qms=1\nother_user_choice=1\n')
 put('boot','first');put('runtime','active');put('maps','/system/lib64/librkses.so\n');put('dependency','unchanged')
 put('module/components/refresh_session/platform.sha256',hashlib.sha256(b'unchanged').hexdigest()+'  '+base+'/dependency\n')
 archive=w/'fixture.tar'
 with tarfile.open(archive,'w') as tf:tf.add(folder,arcname='.')
 assert shell('test ! -e '+base).returncode==0
 subprocess.run(adb+['push',str(archive),base+'.tar'],capture_output=True,check=True)
 assert shell('mkdir '+base+' && tar -xf '+base+'.tar -C '+base).returncode==0
 results=[]
 def case(name,cmd,ok=True):
  r=shell(cmd);assert (r.returncode==0)==ok,(name,r.stdout,r.stderr);results.append(name)
 run='sh '+base+'/module/state.sh '
 case('begin_marks_this_boot',run+'begin && test "$(cat '+base+'/state/pending)" = first')
 case('verify_confirms_removes_pending',run+'verify && test ! -e '+base+'/state/pending && test "$(cat '+base+'/state/qualified)" = first')
 case('fresh_begin',run+'begin')
 case('bad_runtime_rejected','echo failed > '+base+'/runtime; '+run+'verify',False)
 case('explicit_failure_only_disables_session',run+'failed && grep -qx display_session=0 '+base+'/module/config && grep -qx display_qms=1 '+base+'/module/config && grep -qx other_user_choice=1 '+base+'/module/config')
 case('no_module_disable_flag','test ! -e '+base+'/module/disable')
 case('rearm_setup','sed -i s/display_session=0/display_session=1/ '+base+'/module/config; '+run+'begin')
 case('unconfirmed_previous_boot_withdrawn','echo second > '+base+'/boot; '+run+'begin && grep -qx display_session=0 '+base+'/module/config && test ! -e '+base+'/state/pending')
 case('other_choices_still_preserved','grep -qx display_qms=1 '+base+'/module/config && grep -qx other_user_choice=1 '+base+'/module/config')
 case('manual_enable_fresh_boot','sed -i s/display_session=0/display_session=1/ '+base+'/module/config; echo active > '+base+'/runtime; '+run+'begin')
 case('missing_map_rejected','echo unrelated > '+base+'/maps; '+run+'verify',False)
 case('wrong_dependency_rejected','echo /system/lib64/librkses.so > '+base+'/maps; echo wrong > '+base+'/dependency; '+run+'verify',False)
 case('verified_rollback_state_cleanup',run+'failed && test ! -e '+base+'/state/pending && test ! -e '+base+'/module/disable')
 # base is a fixed literal generated above; remove only this confined fixture.
 case('fixture_cleanup','rm -rf '+shlex.quote(base)+'; rm -f '+shlex.quote(base+'.tar')+'; test ! -e '+shlex.quote(base))
 (w/'RESULT.json').write_text(json.dumps({'passed':results,'live_modules_changed':False,'display_IO':False},indent=2)+'\n')
 print('PASS independent session state cases='+str(len(results))+'; confined fake files only')
if __name__=='__main__':main()
