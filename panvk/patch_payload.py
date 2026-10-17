"""Exact-RC3 v10 binary counterpart of the included one-line source fix.

This is not a full Mesa rebuild. Refuse any different binary/instructions.
The 2 instruction changes widen the BO address load and multiply-add; the
existing cs_move64_to helper already accepts the complete 64-bit address.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
import random
import argparse

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--deps-dir', type=Path)
args = parser.parse_args()
if args.deps_dir: sys.path.insert(0, str(args.deps_dir))
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X3, UC_ARM64_REG_X8, UC_ARM64_REG_X20

blob = args.input.read_bytes()
original_hash = hashlib.sha256(blob).hexdigest()
assert original_hash == 'b46b9f0416f207a05482349dffb37b6f3d84c20762151c623774831ce3939445'
eh = struct.unpack_from('<16sHHIQQQIHHHHHH', blob)
assert blob[:6] == b'\x7fELF\x02\x01' and eh[2] == 183
phdrs = [struct.unpack_from('<IIQQQQQQ', blob, eh[5] + i*eh[9]) for i in range(eh[10])]
def offset(va, size):
    matches = [p[2]+va-p[3] for p in phdrs if p[0] == 1 and p[1] & 1
               and p[3] <= va and va+size <= p[3]+p[5]]
    assert len(matches) == 1
    return matches[0]

site = 0xe6e148
file_offset = offset(site, 8)
old = blob[file_offset:file_offset+8]
assert old == struct.pack('<II', 0xb9402908, 0x1b142003)
new = struct.pack('<II', 0xf9401508, 0x9b142003)
md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
assert [(m,o) for _,_,m,o in md.disasm_lite(old,site)] == [
    ('ldr','w8, [x8, #0x28]'), ('madd','w3, w0, w20, w8')]
assert [(m,o) for _,_,m,o in md.disasm_lite(new,site)] == [
    ('ldr','x8, [x8, #0x28]'), ('madd','x3, x0, x20, x8')]
# Verify surrounding draw-context load, bounded stride load and the existing
# cs_move64_to call. Offsets are pinned to this exact release only.
context = blob[offset(0xe6e13c, 0x20):offset(0xe6e13c, 0x20)+0x20]
assert context[:12] == bytes.fromhex('08e959f9140940b9080140f9')
call = struct.unpack_from('<I', blob, offset(0xe6e154,4))[0]
assert call >> 26 == 0b100101
imm = call & 0x3ffffff
if imm & (1 << 25): imm -= 1 << 26
assert 0xe6e154 + imm*4 == 0xe923c8
# cs_move64_to examines bits47:0 and otherwise emits both 32-bit halves.
assert blob[offset(0xe923c8,4):offset(0xe923c8,4)+4] == bytes.fromhex('68fc70d3')

def engine(code):
    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    uc.mem_map(0x100000, 4096); uc.mem_map(0x200000, 4096)
    uc.mem_write(0x100000, code)
    return uc
before, after = engine(old), engine(new)
randomizer = random.Random(3588)
cases = [(base, idx, stride) for base in
    [0,0xfffff000,0x100000000,0x5ffd9e2000,0x800000001000,0xffffff000000]
    for idx in range(16) for stride in (8,128,256,512)]
cases += [(randomizer.randrange(1 << 48) & ~4095,
           randomizer.randrange(16), randomizer.randrange(1,65)*8) for _ in range(4096)]
truncated_cases = 0
for base, idx, stride in cases:
    expected = base + idx*stride
    # This source expression multiplies bounded 32-bit idx*stride first.
    assert idx*stride < 1 << 32 and expected < 1 << 64
    results = []
    for uc in (before, after):
        uc.mem_write(0x200028, struct.pack('<Q',base))
        uc.reg_write(UC_ARM64_REG_X8,0x200000)
        uc.reg_write(UC_ARM64_REG_X0,idx)
        uc.reg_write(UC_ARM64_REG_X20,stride)
        uc.emu_start(0x100000,0x100008)
        results.append(uc.reg_read(UC_ARM64_REG_X3))
        assert uc.reg_read(UC_ARM64_REG_X0) == idx
        assert uc.reg_read(UC_ARM64_REG_X20) == stride
    assert results[0] == expected & 0xffffffff
    assert results[1] == expected
    truncated_cases += results[0] != expected
assert truncated_cases > 4000
patched = bytearray(blob); patched[file_offset:file_offset+8] = new
assert len(patched) == len(blob)
assert [i for i,(a,b) in enumerate(zip(blob,patched)) if a != b] == [
    file_offset+1, file_offset+3, file_offset+7]
out = args.output; out.parent.mkdir(parents=True, exist_ok=True); out.write_bytes(patched)
result = dict(original_sha256=original_hash, patched_sha256=hashlib.sha256(patched).hexdigest(),
    mesa_rebuilt=False, scope='RC3 PAN_ARCH10 only, two AArch64 instructions',
    site_va=hex(site), file_offset=hex(file_offset), old_bytes=old.hex(), new_bytes=new.hex(),
    cases=len(cases), original_truncation_reproduced=truncated_cases, patched_failures=0,
    source_patch='fix-provoking-vertex-address-width.patch')
args.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,indent=2))
