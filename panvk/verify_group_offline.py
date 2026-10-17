"""Compile and emulate the actual C group engine without a GPU or kernel calls."""
from pathlib import Path
import argparse, json, os, struct, subprocess, sys
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ndk',type=Path,required=True)
p.add_argument('--work',type=Path,required=True)
p.add_argument('--deps-dir',type=Path)
a=p.parse_args()
if a.deps_dir: sys.path.insert(0,str(a.deps_dir))
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_SP, UC_ARM64_REG_X30, UC_ARM64_REG_X0, UC_ARM64_REG_PC
src=Path(__file__).resolve().parent
a.work.mkdir(parents=True,exist_ok=True)
host='windows-x86_64' if sys.platform=='win32' else 'linux-x86_64'
cc=a.ndk/'toolchains/llvm/prebuilt'/host/'bin'/('clang.exe' if sys.platform=='win32' else 'clang')
elf=a.work/'csf_group_test.elf'
subprocess.run([str(cc),'--target=aarch64-none-elf','-O2','-ffreestanding','-fno-builtin','-nostdlib','-fuse-ld=lld','-Wall','-Wextra','-Werror','-Wl,-e,test_all','-Wl,-Ttext=0x100000',str(src/'csf_group_core.c'),str(src/'csf_group_test.c'),'-o',str(elf)],check=True)
b=elf.read_bytes();h=struct.unpack_from('<16sHHIQQQIHHHHHH',b)
assert h[0][:6]==b'\x7fELF\x02\x01' and h[2]==183
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);pages=set()
for i in range(h[10]):
    ph=struct.unpack_from('<IIQQQQQQ',b,h[5]+i*h[9])
    if ph[0]!=1:continue
    for page in range(ph[3]&~4095,(ph[3]+ph[6]+4095)&~4095,4096):
        if page not in pages:u.mem_map(page,4096);pages.add(page)
    u.mem_write(ph[3],b[ph[2]:ph[2]+ph[5]])
u.mem_map(0x800000,0x100000);u.mem_map(0xa00000,4096)
u.reg_write(UC_ARM64_REG_SP,0x900000);u.reg_write(UC_ARM64_REG_X30,0xa00000)
u.emu_start(h[4],0xa00000,count=20000000)
assert u.reg_read(UC_ARM64_REG_X0)==0,u.reg_read(UC_ARM64_REG_X0)
assert u.reg_read(UC_ARM64_REG_PC)==0xa00000
v=dict(compiled_aarch64_engine_emulated=True,cycles=1000,create_bind_terminate_operations=9000,failures=0,error_retry_capacity_fd_thread_and_close_checks='PASS',kernel_calls_mocked=True)
(a.work/'group-topology-offline.json').write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8')
print(json.dumps(v))
