"""Stage source-built GPL kernel modules and boot helpers into a Mali installer."""
from pathlib import Path
import hashlib,json,shutil
def stage(destination,kernel,runtime):
 out=Path(destination);kernel=Path(kernel);runtime=Path(runtime)
 k=json.loads((kernel/'BUILD.json').read_text());u=json.loads((runtime/'BUILD.json').read_text())
 (out/'kernel').mkdir(exist_ok=True)
 for key,name in [('candidate','rk_mali_next.ko'),('guard','rk_mali_resource_guard.ko')]:
  p=kernel/name;assert hashlib.sha256(p.read_bytes()).hexdigest()==k[key]['sha256'];shutil.copyfile(p,out/'kernel'/name)
 for name,digest in u.items():
  p=runtime/name;assert hashlib.sha256(p.read_bytes()).hexdigest()==digest;shutil.copyfile(p,out/'bin'/name)
 entries=[*sorted((out/'kernel').glob('*.ko')),*[out/'bin'/n for n in sorted(u)],out/'bin/cold_boot64',*[out/n for n in ['kernel-boot.sh','kernel-state.sh','kernel-transition.sh','kernel-verify.sh','dvfs-state.sh','dvfs-boot.sh','cold-state.sh','cold-verify.sh','zygisk-health.sh','fence-state.sh','root-startup.sh']]]
 (out/'kernel.sha256').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(out).as_posix()+'\n' for p in entries),encoding='utf-8',newline='\n')
 (out/'KERNEL_BUILD.json').write_text(json.dumps({'kernel':k,'runtime':u,'source_license':'GPL-2.0-only for kernel components; file-level licenses retained','original_kernel_image_included':False,'csf_firmware_included':False},indent=2))
