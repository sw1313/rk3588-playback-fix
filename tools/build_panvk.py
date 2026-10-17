"""Package the optional experimental PanVK driver; never connect to a device."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, zipfile
from package_licenses import ROOT, stage
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--payload',type=Path,required=True,help='Exact official Android/aarch64 RC3 .so')
p.add_argument('--ndk',type=Path,required=True)
p.add_argument('--work',type=Path,required=True,help='New build directory, outside the panvk source tree')
p.add_argument('--deps-dir',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.work=a.work.resolve();src=(ROOT/'panvk').resolve()
if a.work==src or src in a.work.parents:p.error('--work must be outside panvk source')
if a.work.exists() and any(a.work.iterdir()):p.error('--work must be empty; no existing files will be deleted')
a.work.mkdir(parents=True,exist_ok=True)
work=a.work/'driver'
shutil.copytree(src,work,ignore=shutil.ignore_patterns('.gitignore','__pycache__','*.so','*.o','*.elf','pydeps'))
def run(script,*args):subprocess.run([sys.executable,str(script),*map(str,args)],check=True)
deps=['--deps-dir',a.deps_dir.resolve()] if a.deps_dir else []
run(work/'patch_payload.py','--input',a.payload.resolve(),'--output',work/'panvk_rc3_payload.so','--report',work/'address-width-offline-validation.json',*deps)
run(work/'verify_group_offline.py','--ndk',a.ndk.resolve(),'--work',a.work/'offline',*deps)
env=dict(os.environ,ANDROID_NDK_HOME=str(a.ndk.resolve()))
subprocess.run([sys.executable,str(work/'build-loader.py')],env=env,check=True)
prov=json.loads((work/'PROVENANCE.txt').read_text(encoding='utf-8'))
for name,expected in prov['testedRuntimeSha256'].items():
    assert hashlib.sha256((work/name).read_bytes()).hexdigest()==expected, 'Runtime differs from tested Test8: '+name
stage(work,'panvk-app')
checks=[]
for f in sorted(work.rglob('*')):
    if f.is_file() and f.suffix not in ('.o','.elf') and f.name!='checksums.sha256':
        checks.append(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(work).as_posix())
(work/'checksums.sha256').write_text('\n'.join(checks)+'\n',encoding='utf-8',newline='\n')
a.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.output,'w',zipfile.ZIP_DEFLATED) as z:
    z.write(work/'meta.json','meta.json')
    for line in checks:
        name=line.split(None,1)[1]
        if name!='meta.json':z.write(work/name,name)
    z.write(work/'checksums.sha256','checksums.sha256')
print(json.dumps(dict(filename=a.output.name,sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),runtime_matches_test8=True)))
