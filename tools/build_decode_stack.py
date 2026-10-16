"""Combine validated component packages without redistributing firmware files."""
import argparse,hashlib,shutil,zipfile
from pathlib import Path
from package_licenses import stage as stage_licenses
ROOT=Path(__file__).resolve().parents[1];MODULE=ROOT/'magisk-decode'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def package(folder,output):
 # Preserve component checksums; only the root manifest is regenerated.
 files=sorted(p for p in folder.rglob('*') if p.is_file() and p!=folder/'checksums.sha256')
 for p in files:
  if p.suffix in {'.sh','.md','.txt','.prop','.sha256','.json','.py','.c','.cpp','.h','.tsv','.xml','.patch'}:p.write_bytes(p.read_bytes().replace(b'\r\n',b'\n'))
 (folder/'checksums.sha256').write_bytes(''.join(sha(p)+'  '+p.relative_to(folder).as_posix()+'\n' for p in files).encode())
 with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
  for p in sorted(folder.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(folder).as_posix())
 print('ZIP',output,'SHA256',sha(output))
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['mpp','rga','codec2','libyuv','software','hdr','work']:parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--codec2-sdk',type=Path,help='Optional SDK14/API32 core; retains the prior Codec2 backend for rollback')
 parser.add_argument('--codec2-sdk-next',type=Path,help='Optional newer core with independent rollback to codec2-sdk')
 a=parser.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True);out=w/'package'
 if a.codec2_sdk_next and not a.codec2_sdk:raise SystemExit('New SDK core requires the prior SDK payload for independent rollback')
 if out.exists():raise SystemExit('Use a fresh package directory')
 shutil.copytree(MODULE,out,ignore=shutil.ignore_patterns('system','components','config','disable','status.txt','active*','checksums.sha256'))
 files=[]
 for name in ['mpp','rga','codec2','libyuv','software','hdr']+(['codec2_sdk'] if a.codec2_sdk else [])+(['codec2_sdk_next'] if a.codec2_sdk_next else []):
  src=getattr(a,name).resolve();dest=out/'components'/name
  for line in (src/'checksums.sha256').read_text(encoding='utf-8').splitlines():
   expected,rel=line.split(None,1)
   if sha(src/rel)!=expected:raise SystemExit('Component input integrity failed: '+name+'/'+rel)
  if name=='codec2':
   allowed={'system/vendor/bin/hw/android.hardware.media.c2@1.1-service','system/vendor/lib/hw/libcodec2_rx_component.so'}
   if {p.relative_to(src).as_posix() for p in (src/'system').rglob('*') if p.is_file()}!=allowed:raise SystemExit('Codec2 input contains generated firmware patches; use the clean pre-install package')
  if name in {'codec2_sdk','codec2_sdk_next'} and {p.relative_to(src).as_posix() for p in (src/'system').rglob('*') if p.is_file()}!={'system/vendor/lib/hw/libcodec2_rx_component.so'}:raise SystemExit('SDK core must contain only the source-built plugin')
  if name=='hdr' and (src/'system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so').exists():raise SystemExit('HDR input contains a generated original HWC; use the clean pre-install package')
  shutil.copytree(src,dest,ignore=shutil.ignore_patterns('disable','status.txt','preflight.txt','mode','resume-codec2'))
  # Normalize nested text before generating its installer manifest. A Windows
  # checkout may otherwise change bytes after the component was checksummed.
  inner=sorted(p for p in dest.rglob('*') if p.is_file() and p!=dest/'checksums.sha256')
  for p in inner:
   if p.suffix in {'.sh','.md','.txt','.prop','.sha256','.json','.py','.c','.cpp','.h','.tsv','.xml','.patch'}:p.write_bytes(p.read_bytes().replace(b'\r\n',b'\n'))
  (dest/'checksums.sha256').write_bytes(''.join(sha(p)+'  '+p.relative_to(dest).as_posix()+'\n' for p in inner).encode())
  for file in (dest/'system').rglob('*'):
   if file.is_file():files.append(sha(file)+'  '+file.relative_to(out).as_posix())
  if name=='codec2':
   for line in (dest/'contracts/patches.txt').read_text(encoding='utf-8').splitlines():
    target,relative,original,patched,recipe=line.split();files.append(patched+'  components/codec2/'+relative)
   files.append('28aaefc10e5b5c270e2b1c464b8cf7fa5cf8b95417e97392543fe1f56ed0175e  components/codec2/system/vendor/etc/media_codecs_c2_base.xml')
  if name=='software':
   files.append('ea14149b401d6f2c3404acb02f8244260a14b1179659a905e49ece338e6224bf  components/software/system/vendor/etc/media_codecs_google_c2.xml')
  if name=='hdr':
   original=(dest/'contracts/original-hwc.sha256').read_text(encoding='utf-8').split()[0]
   files.append(original+'  components/hdr/system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so')
 (out/'files.sha256').write_bytes(('\n'.join(sorted(files))+'\n').encode())
 version=dict(line.split('=',1) for line in (out/'module.prop').read_text(encoding='utf-8').splitlines() if '=' in line)['version']
 stage_licenses(out,'decode-stack')
 package(out,w/('rk3588-decode-stack-'+version+'.zip'))
if __name__=='__main__':main()
