"""Build a Decode cold-QMS trial component from the real kernel and HWC candidates.

Does not install, flash, mark physical acceptance, or bypass the frontend-only
Decode package gate. Firmware images and generated contracts remain private.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('ndk','kernel','hwc','work'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
    report=json.loads((a.kernel/'BUILD.json').read_text())
    ko=a.kernel/'rk_decode_qms.ko';hwc=a.hwc/'package/system/vendor/lib64/hw/hwcomposer.rk30board.so'
    if report.get('kind')!='restricted_cold_QMS_owner_endpoint_candidate' or not report.get('qms_implemented') or sha(ko)!=report['ko_sha256']:
        raise RuntimeError('Real guarded owner/endpoint module required')
    if b'vendor.rkhdr.qms_boot\0' not in hwc.read_bytes():raise RuntimeError('Actual boot-gated HWC frontend required')
    identity=hashlib.sha256(json.dumps({'module':sha(ko),'hwc':sha(hwc),'loader_source':sha(ROOT/'magisk-decode/qms-loader.c')},sort_keys=True).encode()).hexdigest()
    (w/'qms_profile.h').write_text(''.join('#define '+k+' "'+v+'"\n' for k,v in {
        'QMS_PROFILE_ID':identity,'QMS_MODULE_SHA':sha(ko),'QMS_HWC_SHA':sha(hwc)}.items()),encoding='ascii',newline='\n')
    llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt/windows-x86_64/bin'
    out=w/'component';out.mkdir();helper=out/'boot-stage64'
    subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android32','-std=c11','-O2','-static',
        '-Wall','-Wextra','-Werror','-I'+str(w),'-ffile-prefix-map='+ROOT.as_posix()+'=/src',
        str(ROOT/'magisk-decode/qms-loader.c'),'-Wl,-z,relro,-z,now','-o',str(helper)],check=True)
    shutil.copyfile(ko,out/'rk_decode_qms.ko');shutil.copyfile(hwc,out/'hwcomposer.rk30board.so')
    values={'profile_id':identity,'helper_sha256':sha(helper),'module_sha256':sha(ko),
            'hwc_sha256':sha(hwc),'image_sha256':'d9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740'}
    (out/'profile').write_text(''.join(k+' '+v+'\n' for k,v in values.items()),encoding='ascii',newline='\n')
    metadata={'status':'RESTRICTED_COLD_QMS_TRIAL_PAYLOAD','profile':values,'functional_module_built':True,
              'native_import_CRCs':report['canonical_crcs_checked'],'first_graphics_barrier_required':True,
              'boot_loader_supplied':True,'hardware_accepted':False,'physical_HDMI_accepted':False,'release_accepted':False}
    (out/'CANDIDATE.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8',newline='\n')
    (out/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.name+'\n' for f in sorted(out.iterdir()) if f.is_file()),encoding='ascii',newline='\n')
    print(json.dumps({'component':str(out),'profile_id':identity,'hardware_accepted':False}))
if __name__=='__main__':main()
