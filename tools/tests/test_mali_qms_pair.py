"""Exercise the actual platform checker against confined Android fake files.

Only fixture paths/hashes are substituted. No display/GPU, real properties,
module flags or boot partition are touched.
"""
import argparse, hashlib, json, shlex, subprocess, tarfile, re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BASE='/data/local/tmp/rk-qms-pair-fixture'

def main():
    global BASE
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb',required=True);p.add_argument('--serial',required=True)
    p.add_argument('--candidate-profile',type=Path)
    p.add_argument('--port',default='5037');p.add_argument('--work',type=Path,required=True)
    a=p.parse_args();BASE+='-'+hashlib.sha256(str(a.work.resolve()).encode()).hexdigest()[:8]
    a.work.mkdir(parents=True,exist_ok=False);folder=a.work/'fixture';folder.mkdir()
    adb=[a.adb,'-P',a.port,'-s',a.serial]
    def shell(s):return subprocess.run(adb+['shell',s],capture_output=True,text=True,encoding='utf-8',timeout=15)
    if shell('test ! -e '+BASE).returncode:raise RuntimeError('Fixture exists')
    hashes={};files={}
    for manifest in ['unchanged-platform.sha256','decode-sf-target-capture.sha256','decode-sf-target-capture-qms.sha256','decode-sf-target-session-qms.sha256','decode-sf-target-session.sha256']:
        for line in (ROOT/'magisk-mali-global/contracts'/manifest).read_text().splitlines():
            digest,path=line.split();hashes.setdefault(digest,hashlib.sha256(digest.encode()).hexdigest())
            if manifest=='decode-sf-target-capture-qms.sha256':files[path]=digest
    for digest in re.findall(r'qms_(?:module|helper)=([0-9a-f]{64})',(ROOT/'magisk-mali-global/platform-check.sh').read_text()):
        hashes[digest]=hashlib.sha256(digest.encode()).hexdigest()
    def transform(s):
        for old,new in hashes.items():s=s.replace(old,new)
        for old,new in [('/data/adb/modules/rk3588_decode_stack',BASE+'/decode'),('/data/adb/rk3588-decode-qms',BASE+'/state'),('/proc/sys/kernel/random/boot_id',BASE+'/boot'),('/sys/module/rk_decode_qms',BASE+'/owner'),('/system/',BASE+'/system/'),('/vendor/',BASE+'/vendor/')]:s=s.replace(old,new)
        return s
    def put(name,s):
        f=folder/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(s,encoding='utf-8',newline='\n')
    for path,digest in files.items():put(path.lstrip('/'),digest)
    stock='fddde345b957a0898c44cf1c62d23e9736d98d246e46bb3f39244c0e7d6b0d1a'
    put('decode/components/hdr/originals/bin/surfaceflinger',stock)
    put('decode/components/hdr/sf-target10-preflight','a6f27121a627b42f53066756262312b3afe80800271afe14e57923081a72929f')
    # Preflight contains the compiled hash, so transform only this metadata.
    f=folder/'decode/components/hdr/sf-target10-preflight';f.write_text(transform(f.read_text()),encoding='ascii')
    profile='af8c99be2351addbbf36cfe8ec9672e5b5350577dba85f30f46e0e08c4d69105'
    put('decode/components/qms/profile','profile_id '+profile+'\n')
    put('decode/components/qms/rk_decode_qms.ko','e66681590d923d41d0da3565d6bbbbeccd0a81dca2109d0b5755371f49cc0cb6')
    put('decode/components/qms/boot-stage64','4cba587a07dfef61bf6d3dc59e9867bec4cc9ff5b4d725dac786612beb54f00e')
    put('decode/active-boot-id','fixture-boot');put('boot','fixture-boot')
    put('decode/active-config','hwc_client_hdr10=1\ndisplay_qms=1\n')
    put('owner/parameters/owner_online','1');put('state/receipt','fixture-boot '+profile+' prepared')
    put('properties/token','fixture-boot');put('properties/active','active')
    source=transform((ROOT/'magisk-mali-global/platform-check.sh').read_text())
    prop='getprop() { case "$1" in debug.rkhdr.client_hdr10) echo 1;; vendor.rkhdr.qms_boot) cat '+BASE+'/properties/token;; vendor.rkhdr.qms) cat '+BASE+'/properties/active;; esac; }\n'
    source=source.replace('set -eu','set -eu\n'+prop);put('platform-check.sh',source)
    put('contracts/unchanged-platform.sha256',hashes[stock]+'  '+BASE+'/system/bin/surfaceflinger\n')
    for n in ['decode-sf-target-capture','decode-sf-target-capture-qms','decode-sf-target-session-qms','decode-sf-target-session']:put('contracts/'+n+'.sha256',transform((ROOT/'magisk-mali-global/contracts'/(n+'.sha256')).read_text()))
    session_digest=next(row.split()[0] for row in (ROOT/'magisk-mali-global/contracts/decode-sf-target-session-qms.sha256').read_text().splitlines() if row.endswith(' /system/lib64/librkses.so'))
    put('system/lib64/librkses.so',session_digest)
    tar=a.work/'fixture.tar'
    with tarfile.open(tar,'w') as t:t.add(folder,arcname='.')
    subprocess.run(adb+['push',str(tar),BASE+'.tar'],capture_output=True,check=True,timeout=20)
    assert shell('mkdir '+BASE+' && tar -xf '+BASE+'.tar -C '+BASE).returncode==0
    cases=[('baseline',None,None,True),('old_boot','properties/token','old-boot',False),('owner_offline','owner/parameters/owner_online','0',False),('wrong_module','decode/components/qms/rk_decode_qms.ko','wrong',False),('wrong_helper','decode/components/qms/boot-stage64','wrong',False),('wrong_hwc','vendor/lib64/hw/hwcomposer.rk30board.so','wrong',False),('stale_receipt','state/receipt','old-boot '+profile+' prepared',False),('disabled_selection','decode/active-config','hwc_client_hdr10=1\ndisplay_qms=0\n',False),('wrong_profile','decode/components/qms/profile','profile_id wrong\n',False)]
    results=[]
    for name,path,value,expected in cases:
        if path:assert shell('printf %s '+shlex.quote(value)+' > '+BASE+'/'+path).returncode==0
        r=shell('sh '+BASE+'/platform-check.sh');ok=(r.returncode==0)==expected
        results.append({'case':name,'passed':ok});assert ok,(name,r.stdout,r.stderr)
        if path:assert shell('printf %s '+shlex.quote((folder/path).read_text())+' > '+BASE+'/'+path).returncode==0
    new='32252295ca6cf439885a200f8d0b681bd8a7d09ce42cd51dd5c6131cc92bcf0d'
    for path,text in {'decode/components/qms/profile':'profile_id '+new+'\n','state/receipt':'fixture-boot '+new+' prepared','decode/components/qms/rk_decode_qms.ko':'a67dc768e2646a10006f99e5f5a6b6c95fefe5f9b65134853f64d93f37c61181','decode/components/qms/boot-stage64':'35e6a5260798387c2a2b5f62f84c1605c01023f06d2b1872ca37f391c1a5b73b'}.items():
        assert shell('printf %s '+shlex.quote(text)+' > '+BASE+'/'+path).returncode==0
    r=shell('sh '+BASE+'/platform-check.sh');assert r.returncode==0, r.stdout+r.stderr
    results.append({'case':'new_exact_pair','passed':True})
    assert shell('printf %s '+shlex.quote((folder/'decode/components/qms/rk_decode_qms.ko').read_text())+' > '+BASE+'/decode/components/qms/rk_decode_qms.ko').returncode==0
    assert shell('sh '+BASE+'/platform-check.sh').returncode!=0
    results.append({'case':'reject_mixed_old_module_new_profile','passed':True})
    latest='06dd246e478f3a5d0076b4e284f812aed617f55281384c20f0b11f0abf859d4e'
    for path,text in {'decode/components/qms/profile':'profile_id '+latest+'\n','state/receipt':'fixture-boot '+latest+' prepared','decode/components/qms/rk_decode_qms.ko':'af570721365c144b0e3b01c3c527f9fba295d37fadec9a204426fea80ab57571','decode/components/qms/boot-stage64':'606177689ce43b55850bd5d31dc57de5c7d62d6df8be12bf5bd431e4a52d4d47'}.items():
        assert shell('printf %s '+shlex.quote(text)+' > '+BASE+'/'+path).returncode==0
    assert shell('sh '+BASE+'/platform-check.sh').returncode==0
    results.append({'case':'observation_exact_pair','passed':True})
    assert shell('printf %s '+shlex.quote((folder/'decode/components/qms/boot-stage64').read_text())+' > '+BASE+'/decode/components/qms/boot-stage64').returncode==0
    assert shell('sh '+BASE+'/platform-check.sh').returncode!=0
    results.append({'case':'reject_mixed_observation_module_old_helper','passed':True})
    if a.candidate_profile:
        candidate=dict(row.split() for row in a.candidate_profile.read_text().splitlines())
        profile=candidate['profile_id']
        for path,text in {'decode/components/qms/profile':'profile_id '+profile+'\n','state/receipt':'fixture-boot '+profile+' prepared','decode/components/qms/rk_decode_qms.ko':candidate['module_sha256'],'decode/components/qms/boot-stage64':candidate['helper_sha256']}.items():
            assert shell('printf %s '+shlex.quote(text)+' > '+BASE+'/'+path).returncode==0
        r=shell('sh '+BASE+'/platform-check.sh');assert r.returncode==0,r.stdout+r.stderr
        results.append({'case':'candidate_exact_pair','passed':True})
        assert shell('printf %s '+shlex.quote((folder/'decode/components/qms/boot-stage64').read_text())+' > '+BASE+'/decode/components/qms/boot-stage64').returncode==0
        assert shell('sh '+BASE+'/platform-check.sh').returncode!=0
        results.append({'case':'reject_candidate_mixed_helper','passed':True})
        assert shell('printf %s '+shlex.quote(candidate['helper_sha256'])+' > '+BASE+'/decode/components/qms/boot-stage64').returncode==0
        for path,text in {'system/lib64/libsfhdr.so':'1386aefcdc263c61c0c04e7d952ddaf8cd665eb119b91d3c3c3abfc267ac787e','decode/active-config':'hwc_client_hdr10=1\ndisplay_qms=1\ndisplay_session=1\n'}.items():
            assert shell('printf %s '+shlex.quote(text)+' > '+BASE+'/'+path).returncode==0
        r=shell('sh '+BASE+'/platform-check.sh');assert r.returncode==0,r.stdout+r.stderr
        results.append({'case':'session_exact_tuple','passed':True})
        for name,path,text in [('session_missing_choice','decode/active-config','hwc_client_hdr10=1\ndisplay_qms=1\ndisplay_session=0\n'),('session_bad_library','system/lib64/librkses.so','wrong'),('session_wrong_hdr_pair','vendor/lib64/libga10.so','wrong')]:
            original='hwc_client_hdr10=1\ndisplay_qms=1\ndisplay_session=1\n' if path=='decode/active-config' else (folder/path).read_text()
            assert shell('printf %s '+shlex.quote(text)+' > '+BASE+'/'+path).returncode==0
            r=shell('sh '+BASE+'/platform-check.sh');assert r.returncode!=0,(name,r.stdout,r.stderr)
            results.append({'case':name,'passed':True})
            assert shell('printf %s '+shlex.quote(original)+' > '+BASE+'/'+path).returncode==0
        # Matched-rate retention also works independently of the QMS frontend.
        for path,value in {'vendor/lib64/hw/hwcomposer.rk30board.so':'77030ef54b8c8146966c1926f81d34fe309221f5d54344c54b2c7acba4767e5c','decode/active-config':'hwc_client_hdr10=1\ndisplay_qms=0\ndisplay_session=1\n','properties/active':'inactive','state/receipt':'unrelated'}.items():
            assert shell('printf %s '+shlex.quote(value)+' > '+BASE+'/'+path).returncode==0
        assert shell('sh '+BASE+'/platform-check.sh').returncode==0
        results.append({'case':'session_normal_hwc_without_qms','passed':True})
        for name,path,value in [('normal_bad_library','system/lib64/librkses.so','wrong'),('normal_bad_loader','system/lib64/libsfhdr.so','wrong'),('normal_missing_selection','decode/active-config','hwc_client_hdr10=1\ndisplay_qms=0\ndisplay_session=0\n')]:
            old=shell('cat '+BASE+'/'+path).stdout
            assert shell('printf %s '+shlex.quote(value)+' > '+BASE+'/'+path).returncode==0
            assert shell('sh '+BASE+'/platform-check.sh').returncode!=0
            results.append({'case':name,'passed':True})
            assert shell('printf %s '+shlex.quote(old)+' > '+BASE+'/'+path).returncode==0
    (a.work/'RESULT.json').write_text(json.dumps({'cases':results,'display_IO':False,'actual_checker_exercised':True},indent=2)+'\n')
    print('PASS actual platform checker cases='+str(len(results))+'; confined fake files only')

if __name__=='__main__':main()
