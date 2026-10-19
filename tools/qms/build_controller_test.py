"""Build Android pthread controller tests; injected native ops, no display IO."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ndk',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
ndk=a.ndk.resolve();llvm=ndk/'toolchains/llvm/prebuilt/windows-x86_64'
sources=[ROOT/'decode-kernel/qms'/(n+'.c') for n in ('core','transaction','session','controller','endpoint')]
sources += [ROOT/'tools/qms/controller_test.c']
binary=w/'controller-test64'
subprocess.run([str(llvm/'bin/clang.exe'),'--target=aarch64-linux-android32',
    '--sysroot='+str(llvm/'sysroot'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-pthread',
    '-D_POSIX_C_SOURCE=200809L','-I'+str(ROOT/'tools/qms/controller-fixture'),
    '-I'+str(ROOT/'decode-kernel/qms'),*map(str,sources),'-o',str(binary)],check=True)
inputs=[*sources,ROOT/'decode-kernel/qms/controller.h',*sorted((ROOT/'tools/qms/controller-fixture').rglob('*.h'))]
report={'status':'BUILT_NOT_EXECUTED','display_IO':False,'native_ops':'injected',
    'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
    'source_sha256':{f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}}
(w/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
