"""Run actual session and HWC policy C code on ARM64, no device I/O."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ndk', type=Path, required=True)
    p.add_argument('--python-libs', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--engine-policy', action='store_true',
                   help='Test actual native-engine publication policy with injected low-level operations')
    p.add_argument('--lifecycle-policy', action='store_true',help='Test actual native-event sidecar policy, no hardware')
    p.add_argument('--cached-private-edid', type=Path,
                   help='Optional private cache; tests parser only, never proves current link readiness')
    a = p.parse_args()
    if a.engine_policy and a.lifecycle_policy: raise RuntimeError('Select one policy fixture')
    sys.path.insert(0, str(a.python_libs.resolve()))
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_PROT_ALL
    from unicorn.arm64_const import UC_ARM64_REG_SP, UC_ARM64_REG_X30, UC_ARM64_REG_X0, UC_ARM64_REG_PC
    work = a.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    clang = a.ndk.resolve() / 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
    flags = ['-std=c11', '-O2', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
             '-mgeneral-regs-only', '-nostdlib', '-static', '-Wall', '-Wextra', '-Werror',
             '-I' + str(ROOT / 'decode-kernel/qms'), '-I' + str(work)]
    if a.cached_private_edid:
        raw = a.cached_private_edid.read_bytes()
        if not raw or len(raw) > 32768:
            raise RuntimeError('Invalid cached EDID input size')
        (work / 'private_edid.h').write_text('static const unsigned char private_edid[] = {' +
            ','.join(str(b) for b in raw) + '};\n', encoding='ascii')
        flags += ['-DRKQMS_PRIVATE_EDID=1']
    sources = [ROOT / 'decode-kernel/qms' / (name + '.c') for name in ('core', 'edid', 'transaction', 'session','lifecycle')]
    test_source = ROOT / 'tools/qms' / ('lifecycle_test.c' if a.lifecycle_policy else 'engine_test.c' if a.engine_policy else 'session_test.c')
    if a.engine_policy:
        flags += ['-I' + str(ROOT / 'tools/qms/fixture')]
    binary = work / 'session-test.elf'
    subprocess.run([str(clang), '--target=aarch64-linux-android32', *flags,
                    '-Wl,--no-undefined,-e,main,-Ttext=0x1000000', *map(str, sources),
                    str(test_source), '-o', str(binary)], check=True)
    # Same UAPI shape at the 32-bit userspace boundary. No native private kernel
    # structures or ARM64-only pointers appear in these syscall objects.
    arm32_flags = [f for f in flags if f != '-mgeneral-regs-only']
    subprocess.run([str(clang), '--target=armv7a-linux-androideabi32', *arm32_flags,
                    '-c', str(test_source), '-o', str(work / 'uapi-arm32.o')], check=True)
    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    pages = set()
    with binary.open('rb') as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment['p_type'] != 'PT_LOAD':
                continue
            start, size = segment['p_vaddr'], segment['p_memsz']
            for page in range(start & ~4095, (start + size + 4095) & ~4095, 4096):
                if page not in pages:
                    uc.mem_map(page, 4096, UC_PROT_ALL)
                    pages.add(page)
            uc.mem_write(start, segment.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    uc.mem_map(0x4000000, 0x200000, UC_PROT_ALL)
    uc.mem_map(0x5000000, 4096, UC_PROT_ALL)
    uc.reg_write(UC_ARM64_REG_SP, 0x4200000 - 16)
    uc.reg_write(UC_ARM64_REG_X30, 0x5000000)
    started = time.monotonic()
    uc.emu_start(symbols['main'], 0x5000000, timeout=30_000_000, count=100_000_000)
    if uc.reg_read(UC_ARM64_REG_PC) != 0x5000000:
        raise RuntimeError('Session test execution budget exhausted')
    if uc.reg_read(UC_ARM64_REG_X0):
        raise RuntimeError('Session test failed at C line ' + str(uc.reg_read(UC_ARM64_REG_X0)))
    if a.lifecycle_policy:
        report = {'native_lifecycle_cases': struct.unpack('<I',uc.mem_read(symbols['lifecycle_test_cases'],4))[0],
                  'native_callbacks_and_register_IO':'not invoked; policy only'}
    elif a.engine_policy:
        report = {'native_engine_publication_cases': struct.unpack('<I', uc.mem_read(symbols['engine_test_cases'], 4))[0],
                  'native_registers_and_factory_commit': 'injected; not kernel or hardware acceptance'}
    else:
        counts = struct.unpack('<5I', uc.mem_read(symbols['test_results'], 20))
        report = dict(zip(('request_gate_cases', 'lifecycle_cases', 'HWC_policy_cases', 'rate_cases',
                           'cached_private_EDID_parser_cases'), counts))
    hashed = [*sources, ROOT / 'decode-kernel/qms/uapi.h', ROOT / 'magisk-hdr/src/qms_client.h']
    if a.engine_policy: hashed += [ROOT / 'decode-kernel/qms/native_engine.h',test_source]
    if a.lifecycle_policy: hashed += [ROOT / 'decode-kernel/qms/lifecycle.h',test_source]
    report.update(status='OFFLINE_ACTUAL_C_PASSED_NO_NATIVE_BINDING', device_IO=False,
                  physical_HDMI_acceptance=False, UAPI_ARM32_and_ARM64_layout_checked=not (a.engine_policy or a.lifecycle_policy),
                  elapsed_seconds=round(time.monotonic()-started, 3),
                  source_sha256={str(f.relative_to(ROOT)).replace('\\', '/'): hashlib.sha256(f.read_bytes()).hexdigest() for f in hashed})
    (work / 'RESULT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
