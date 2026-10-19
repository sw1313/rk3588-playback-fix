"""Compile and execute the real QMS C core under ARM64 emulation; no device IO."""
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
    a = p.parse_args()
    sys.path.insert(0, str(a.python_libs.resolve()))
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_PROT_ALL
    from unicorn.arm64_const import UC_ARM64_REG_SP, UC_ARM64_REG_X30, UC_ARM64_REG_X0, UC_ARM64_REG_PC
    work = a.work.resolve(); work.mkdir(parents=True, exist_ok=True)
    clang = a.ndk.resolve() / 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
    binary = work / 'qms-core-test.elf'
    command = [clang, '--target=aarch64-linux-android32', '-std=c11', '-O2', '-ffreestanding',
               '-fno-builtin', '-fno-stack-protector', '-mgeneral-regs-only', '-nostdlib', '-static',
               '-Wall', '-Wextra', '-Werror', '-Wl,--no-undefined,-e,main,-Ttext=0x1000000',
               '-I' + str(ROOT / 'decode-kernel/qms'), ROOT / 'decode-kernel/qms/core.c',
               ROOT / 'decode-kernel/qms/edid.c', ROOT / 'decode-kernel/qms/transaction.c',
               ROOT / 'tools/qms/core_test.c', '-o', binary]
    subprocess.run([str(x) for x in command], check=True)
    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    pages = set()
    with binary.open('rb') as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment['p_type'] != 'PT_LOAD': continue
            start, size = segment['p_vaddr'], segment['p_memsz']
            for page in range(start & ~4095, (start + size + 4095) & ~4095, 4096):
                if page not in pages: uc.mem_map(page, 4096, UC_PROT_ALL); pages.add(page)
            uc.mem_write(start, segment.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
        entry = symbols['main']; result_address = symbols['test_results']
    uc.mem_map(0x4000000, 0x200000, UC_PROT_ALL)
    uc.mem_map(0x5000000, 4096, UC_PROT_ALL)
    uc.reg_write(UC_ARM64_REG_SP, 0x4200000 - 16)
    uc.reg_write(UC_ARM64_REG_X30, 0x5000000)
    start = time.monotonic()
    uc.emu_start(entry, 0x5000000, timeout=60_000_000, count=400_000_000)
    pc = uc.reg_read(UC_ARM64_REG_PC); result = uc.reg_read(UC_ARM64_REG_X0)
    if pc != 0x5000000:
        raise RuntimeError('ARM64 test budget exhausted before completion')
    if result:
        raise RuntimeError('QMS core test failed at C line ' + str(result))
    counts = struct.unpack('<6I', uc.mem_read(result_address, 24))
    report = dict(zip(('exact_rational_table_cycles', 'switch_and_restore_scenarios', 'stable_frames', 'negative_cases',
                       'injected_transaction_failures', 'EDID_capability_cases'), counts))
    report.update(status='OFFLINE_ACTUAL_CORE_PASSED_NO_HARDWARE', physical_HDMI_acceptance=False,
                  elapsed_seconds=round(time.monotonic() - start, 3),
                  core_sha256=hashlib.sha256((ROOT / 'decode-kernel/qms/core.c').read_bytes()).hexdigest())
    (work / 'RESULT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
