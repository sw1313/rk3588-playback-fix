"""Build a boot-validation-only Mali package from the pinned public release.

Reuses unchanged GPU/KMD/interface bytes from the public ZIP, never an installed
module directory. Rebuilds the native keeper and includes all changed source.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,zipfile
from package_licenses import stage as stage_licenses

ROOT=Path(__file__).resolve().parents[1]
BASE_SHA='7ab0ef96f8560dd1c732df353879c26b567ce0e20c28116f95c1061033aa4a0c'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def require(ok,message):
    if not ok:raise ValueError(message)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('base','ndk','work'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--qms-barrier',action='store_true',help='Retain the existing pre-graphics Decode QMS coordination')
    p.add_argument('--source-ref',default='working-tree',help='Published source tag, or working-tree for an unreleased build')
    a=p.parse_args()
    require(sha(a.base)==BASE_SHA,'Expected unchanged public Mali Global 0.4.2 ZIP')
    work=a.work.resolve();require(not work.exists(),'Use a fresh work directory');work.mkdir(parents=True)
    out=work/'package'
    with zipfile.ZipFile(a.base) as z:
        require(z.testzip() is None,'Corrupt base ZIP')
        for name in z.namelist():
            require(not Path(name).is_absolute() and '..' not in Path(name).parts,'Unsafe ZIP path')
        z.extractall(out)
    # Verify the baseline before replacing any source-built policy files.
    for row in (out/'checksums.sha256').read_text().splitlines():
        expected,name=row.split(None,1);require(sha(out/name.strip())==expected,'Base package checksum differs')
    subprocess.run([sys.executable,ROOT/'tools/cold-boot/generate_contract.py','--check'],check=True)
    changed=['module.prop','boot-stage.sh','cold-state.sh','root-startup.sh','boot/README.md','README.md','src/cold_boot.c',
             'src/cold_sha256.h','src/cold_firmware.h','src/cold_firmware_contract.h','contracts/boot-firmware.json',
             'webui.sh','webroot/app.js','webroot/index.html','webroot/i18n.js']
    for name in changed:
        target=out/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes((ROOT/'magisk-mali-global'/name).read_bytes().replace(b'\r\n',b'\n'))
    host='windows-x86_64' if os.name=='nt' else 'linux-x86_64';ext='.exe' if os.name=='nt' else ''
    clang=a.ndk/'toolchains/llvm/prebuilt'/host/'bin'/('clang'+ext)
    subprocess.run([str(clang),'--target=aarch64-linux-android32','-std=c11','-O2','-static','-Wall','-Wextra','-Werror',
                    *(['-DRKQMS_DISPLAY_STAGE=1'] if a.qms_barrier else []),
                    '-ffile-prefix-map='+ROOT.as_posix()+'=/src',str(ROOT/'magisk-mali-global/src/cold_boot.c'),
                    '-Wl,-z,relro,-z,now','-o',str(out/'bin/cold_boot64')],check=True)
    names=[row.split(None,1)[1].strip() for row in (out/'kernel.sha256').read_text().splitlines()]
    names+=['root-startup.sh']
    (out/'kernel.sha256').write_text(''.join(sha(out/n)+'  '+n+'\n' for n in names),encoding='utf-8',newline='\n')
    stage_licenses(out,'mali-global',source_ref=a.source_ref)
    version=dict(line.split('=',1) for line in (out/'module.prop').read_text().splitlines() if '=' in line)['version']
    evidence=json.loads((ROOT/'docs/validation/mali-boot-compat-043.json').read_text(encoding='utf-8'))
    root_evidence=json.loads((ROOT/'docs/validation/root-interface-compat-044.json').read_text(encoding='utf-8'))
    formal=a.source_ref!='working-tree'
    if formal:
        require(a.source_ref=='v0.8.8' and version=='0.4.4','Formal source tag/version differs')
        require(evidence['new_first_binding_acceptance'] is True,'Actual first-binding acceptance missing')
        require(root_evidence['actual_device']['candidate_full_installer'] is True,'Root-interface installer acceptance missing')
        require(root_evidence['actual_device']['candidate_reboot_first_binding_and_qualification'] is True,'Root-interface reboot acceptance missing')
        require(sha(out/'bin/cold_boot64')==evidence['tested_qms_keeper_sha256'],'Formal keeper differs from actual tested keeper')
        require(a.qms_barrier,'The accepted formal package includes Decode startup coordination')
    validation={'component':'mali-global','version':version,
                'status':'FIRMWARE_PINNED_RELEASE' if formal else 'UNRELEASED_BOOT_COMPATIBILITY_CANDIDATE',
                'base_release_sha256':BASE_SHA,'gpu_kmd_csf_unchanged':True,
                'new_first_binding_acceptance':evidence['new_first_binding_acceptance'] if formal else False,
                'native_keeper_prior_hardware_acceptance':sha(out/'bin/cold_boot64')==evidence['tested_qms_keeper_sha256'],
                'hardware_scope':'See version-specific evidence; existing keeper hardware acceptance does not certify new root implementations',
                'evidence':'docs/validation/release-0.8.8.json' if formal else 'docs/validation/root-interface-compat-044.json',
                'qms_barrier_included':a.qms_barrier,'keeper_sha256':sha(out/'bin/cold_boot64'),'source_ref':a.source_ref}
    validation.update(root_admission='interface_and_local_layout',root_version_whitelist=False,
                      root_binary_hash_whitelist=False,new_root_variant_hardware_acceptance=False,
                      kernelsu_apatch_boot_backend_supported=False)
    (out/'VALIDATION.json').write_text(json.dumps(validation,indent=2)+'\n',encoding='utf-8')
    files=sorted(f for f in out.rglob('*') if f.is_file() and f.name!='checksums.sha256')
    (out/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in files),encoding='utf-8',newline='\n')
    version=dict(line.split('=',1) for line in (out/'module.prop').read_text().splitlines() if '=' in line)['version']
    dest=work/('rk3588-mali-global-'+version+'.zip')
    with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
        for f in sorted(out.rglob('*')):
            if f.is_file():z.write(f,f.relative_to(out).as_posix())
    with zipfile.ZipFile(dest) as z:require(z.testzip() is None,'Candidate ZIP failed verification')
    print(dest,sha(dest))

if __name__=='__main__':main()
