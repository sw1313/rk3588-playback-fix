"""Compile actual native register/lifecycle C against injected object/I/O fixture."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt/windows-x86_64'
binary=w/'native-lifecycle-test64'
sources=[ROOT/'decode-kernel/qms/lifecycle.c',ROOT/'decode-kernel/qms/core.c',ROOT/'tools/qms/native_lifecycle_test.c']
subprocess.run([str(llvm/'bin/clang.exe'),'--target=aarch64-linux-android32','--sysroot='+str(llvm/'sysroot'),
 '-std=c11','-O2','-Wall','-Wextra','-Werror','-pthread','-D_POSIX_C_SOURCE=200809L',
 '-I'+str(ROOT/'tools/qms/controller-fixture'),'-I'+str(ROOT/'decode-kernel/qms'),*map(str,sources),'-o',str(binary)],check=True)
inputs=[*sources,*[ROOT/'decode-kernel/qms'/n for n in ['native_pm.h','native_lifecycle.h','native_registers.h','lifecycle.h']],
        *sorted((ROOT/'tools/qms/controller-fixture').rglob('*.h'))]
j={'status':'BUILT_NOT_EXECUTED','binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'display_IO':False,
   'native_object_layout_and_IO':'injected; not kernel ABI proof',
   'source_sha256':{f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}}
(w/'BUILD.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps(j,indent=2))
