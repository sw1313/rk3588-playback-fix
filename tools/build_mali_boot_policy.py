"""Build a boot-validation-only Mali candidate from the pinned public release.

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
    changed=['module.prop','boot-stage.sh','boot/README.md','README.md','src/cold_boot.c',
             'src/cold_sha256.h','src/cold_firmware.h','src/cold_firmware_contract.h','contracts/boot-firmware.json']
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
    (out/'kernel.sha256').write_text(''.join(sha(out/n)+'  '+n+'\n' for n in names),encoding='utf-8',newline='\n')
    stage_licenses(out,'mali-global',source_ref='working-tree')
    validation={'component':'mali-global','version':'0.4.3-test1','status':'UNRELEASED_BOOT_COMPATIBILITY_CANDIDATE',
                'base_release_sha256':BASE_SHA,'gpu_kmd_csf_unchanged':True,'boot_written':False,
                'new_first_binding_acceptance':False,'qms_barrier_included':a.qms_barrier,'source_ref':'working-tree'}
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
