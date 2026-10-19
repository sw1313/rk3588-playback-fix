"""Build actual native link qualifier with test-only native objects and I/O."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--ndk', type=Path, required=True)
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
w = a.work.resolve()
w.mkdir(parents=True, exist_ok=False)
llvm = a.ndk.resolve() / 'toolchains/llvm/prebuilt/windows-x86_64'
binary = w / 'native-link-test64'
sources = [ROOT / 'decode-kernel/qms' / (n + '.c') for n in ('core', 'edid', 'lifecycle', 'native_link', 'native_qualify')]
sources += [ROOT / 'tools/qms/native_link_test.c']
subprocess.run([str(llvm / 'bin/clang.exe'), '--target=aarch64-linux-android32',
    '--sysroot=' + str(llvm / 'sysroot'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-pthread', '-D_POSIX_C_SOURCE=200809L', '-I' + str(ROOT / 'tools/qms/controller-fixture'),
    '-I' + str(ROOT / 'decode-kernel/qms'), *map(str, sources), '-o', str(binary)], check=True)
inputs = [*sources, ROOT / 'decode-kernel/qms/native_link.h',
          *sorted((ROOT / 'tools/qms/controller-fixture').rglob('*.h'))]
j = {'status': 'BUILT_NOT_EXECUTED', 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
     'display_IO': False, 'native_object_layout_and_IO': 'injected; not kernel ABI proof',
     'source_sha256': {f.relative_to(ROOT).as_posix(): hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}}
(w / 'BUILD.json').write_text(json.dumps(j, indent=2) + '\n')
print(json.dumps({'binary': str(binary), 'sha256': j['binary_sha256']}))
