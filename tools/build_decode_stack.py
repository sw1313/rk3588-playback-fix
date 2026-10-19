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
 parser.add_argument('--codec2-pool',type=Path,help='Optional internal ARM64 P010 framework pool adapter; default-off boot selection')
 parser.add_argument('--hdmi-event',type=Path,help='Optional internal ARM64 HDMI transient event component; default-off reboot selection')
 parser.add_argument('--ddc',type=Path,help='Exact-image native HDMI E-DDC repair component, default off')
 parser.add_argument('--qms',type=Path,help='Complete guarded cold-QMS trial component; separate from the normal HDR package, default off')
 parser.add_argument('--display-policy',type=Path,help='Source-only exact-firmware locale/fixed-mode component, generated privately at install, default off')
 parser.add_argument('--refresh-session',type=Path,help='Exact SF fixed-source session policy; independent reboot rollback, default off')
 parser.add_argument('--source-ref',help='Source tag/ref; working-tree marks an unpublished local candidate')
 a=parser.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True);out=w/'package'
 if a.codec2_sdk_next and not a.codec2_sdk:raise SystemExit('New SDK core requires the prior SDK payload for independent rollback')
 if out.exists():raise SystemExit('Use a fresh package directory')
 shutil.copytree(MODULE,out,ignore=shutil.ignore_patterns('system','components','config','disable','status.txt','active*','checksums.sha256'))
 files=[]
 for name in ['mpp','rga','codec2','libyuv','software','hdr']+(['codec2_sdk'] if a.codec2_sdk else [])+(['codec2_sdk_next'] if a.codec2_sdk_next else [])+(['codec2_pool'] if a.codec2_pool else [])+(['hdmi_event'] if a.hdmi_event else [])+(['display_policy'] if a.display_policy else [])+(['refresh_session'] if a.refresh_session else []):
  src=getattr(a,name).resolve();dest=out/'components'/name
  for line in (src/'checksums.sha256').read_text(encoding='utf-8').splitlines():
   expected,rel=line.split(None,1)
   if sha(src/rel)!=expected:raise SystemExit('Component input integrity failed: '+name+'/'+rel)
  if name=='codec2':
   allowed={'system/vendor/bin/hw/android.hardware.media.c2@1.1-service','system/vendor/lib/hw/libcodec2_rx_component.so'}
   if {p.relative_to(src).as_posix() for p in (src/'system').rglob('*') if p.is_file()}!=allowed:raise SystemExit('Codec2 input contains generated firmware patches; use the clean pre-install package')
  if name in {'codec2_sdk','codec2_sdk_next'} and {p.relative_to(src).as_posix() for p in (src/'system').rglob('*') if p.is_file()}!={'system/vendor/lib/hw/libcodec2_rx_component.so'}:raise SystemExit('SDK core must contain only the source-built plugin')
  if name=='hdr':
   if (src/'system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so').exists():raise SystemExit('HDR input contains a generated original HWC; use the clean pre-install package')
   hwc=src/'system/vendor/lib64/hw/hwcomposer.rk30board.so'
   if (src/'QMS_CANDIDATE.json').exists() or (hwc.exists() and b'vendor.rkhdr.qms_boot\0' in hwc.read_bytes()):
    raise SystemExit('QMS frontend has no validated native binding/boot payload; refusing an installable Decode Stack package')
  if name=='codec2_pool' and ({p.relative_to(src).as_posix() for p in (src/'system').rglob('*') if p.is_file()}!={'system/lib64/libcp10.so'} or (src/'variants').exists()):raise SystemExit('P010 pool input must contain only its source-built shim, never a firmware variant')
  if name=='hdmi_event' and ((src/'system').exists() or (src/'originals').exists() or (src/'variants').exists()):raise SystemExit('HDMI input must not contain generated firmware copies')
  if name=='display_policy':
   import json
   if any((src/n).exists() for n in ['system','originals','variants','preflight','services.jar','oat.sha256','oat-inputs.sha256']):raise SystemExit('Display policy must not contain private firmware copies or generated preflight')
   meta=json.loads((src/'CANDIDATE.json').read_text(encoding='utf-8'))
   if meta.get('firmware_services_sha256')!='922c443c3a2b609b7f7f3755bbfbeb90f3c0b0c141829f3b6151371c2aa607b6' or meta.get('private_firmware_bundled') is not False:raise SystemExit('Unknown display policy firmware contract')
  if name=='refresh_session':
   import json
   meta=json.loads((src/'CANDIDATE.json').read_text(encoding='utf-8'))
   if meta.get('firmware_sf_sha256')!='fddde345b957a0898c44cf1c62d23e9736d98d246e46bb3f39244c0e7d6b0d1a' or meta.get('app_hooks') is not False:raise SystemExit('Unknown system session policy')
   if {p.relative_to(src).as_posix() for p in (src/'system').rglob('*') if p.is_file()}!={'system/lib64/libsfhdr.so','system/lib64/librkses.so'}:raise SystemExit('Unexpected session payload')
   if sha(src/'system/lib64/libsfhdr.so')!=meta['loader_sha256'] or sha(src/'system/lib64/librkses.so')!=meta['library_sha256']:raise SystemExit('Session loader tuple differs')
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
  if name=='codec2_pool':
   files.append('f9e01d5ff014ab42a40ddaafee6f3df8122502dd1f0641de58ae75b1b96610dd  components/codec2_pool/variants/lib64/libsfplugin_ccodec.so')
  if name=='hdmi_event':
   import json
   contract=json.loads((dest/'contracts/runtime.json').read_text(encoding='utf-8'))
   files.append(contract['patched_sha256']+'  components/hdmi_event/variants/libandroid_runtime.so')
   files.append(contract['original_sha256']+'  components/hdmi_event/originals/libandroid_runtime.so')
   files.append(sha(dest/'target/libhevt.so')+'  components/hdmi_event/target/libhevt.so')
  if name=='hdr':
   if (src/'originals').exists() or (src/'variants').exists():raise SystemExit('HDR input must not include generated firmware copies')
   files.append(sha(dest/'target/libsfhdr.so')+'  components/hdr/target/libsfhdr.so')
   files.append(sha(dest/'target/libga10.so')+'  components/hdr/target/libga10.so')
   files.append('78bd95f083a08bdf36ed266d9a9ae9923d4d3185e9d33dcaa1ddcbb7baa9fa3b  components/hdr/originals/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so')
   files.append('49a69a0601883b1bccf7d979e891a049c1a30a9a66596cb47a57fc903ac7bc58  components/hdr/variants/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so')
   files.append('fddde345b957a0898c44cf1c62d23e9736d98d246e46bb3f39244c0e7d6b0d1a  components/hdr/originals/bin/surfaceflinger')
   files.append('a6f27121a627b42f53066756262312b3afe80800271afe14e57923081a72929f  components/hdr/variants/bin/surfaceflinger')
   original=(dest/'contracts/original-hwc.sha256').read_text(encoding='utf-8').split()[0]
   files.append(original+'  components/hdr/system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so')
   files.append('47702a172f0b95ecc88dbc60a9b42a09b80841e9b4269ce74d7af916af33901b  components/hdr/variants/hwcomposer.rk30board.scaled.so')
   files.append('8e812c303735807beb570a786065ddd9b7ce448c68d4d2a2c86236d8705b4ea7  components/hdr/originals/lib64/libhwui.so')
   files.append('870f4e31327e7c13269c81cf1c71835c32e2c5244d13fdd5c1360d1585ed6d6c  components/hdr/variants/lib64/libhwui.so')
   files.append('ce91abbde54db81e9e70a13188a7cd32bcedee4eda29534d7b1f56c4f601afa2  components/hdr/originals/lib/libhwui.so')
   files.append('32f0d5d578eac30884d783bc83aea565175a89e1c56acaedf1c1ab1865d0f644  components/hdr/variants/lib/libhwui.so')
 if a.ddc:
  import json
  src=a.ddc.resolve();meta=json.loads((src/'CANDIDATE.json').read_text(encoding='utf-8'))
  if meta.get('kind')!='exact_image_Decode_E_DDC_and_native_initialization':raise SystemExit('Complete native DDC component required')
  for line in (src/'checksums.sha256').read_text(encoding='ascii').splitlines():
   digest,relative=line.split(None,1)
   if Path(relative).is_absolute() or '..' in Path(relative).parts or sha(src/relative)!=digest:raise SystemExit('DDC component input integrity failed')
  for name in ['rk_decode_ddc.ko','rk_decode_ddc_recover.ko']:
   if sha(src/name)!=meta['artifacts'].get(name):raise SystemExit('DDC native artifact mismatch')
  dest=out/'components/ddc';shutil.copytree(src,dest)
  files.extend(sha(f)+'  components/ddc/'+f.name for f in sorted(dest.iterdir()) if f.is_file())
  with (out/'config').open('a',encoding='ascii') as f:f.write('display_ddc=0\n')
 if a.qms:
  import json
  src=a.qms.resolve();meta=json.loads((src/'CANDIDATE.json').read_text(encoding='utf-8'))
  for line in (src/'checksums.sha256').read_text(encoding='ascii').splitlines():
   expected,relative=line.split(None,1)
   if Path(relative).is_absolute() or '..' in Path(relative).parts or sha(src/relative)!=expected:
    raise SystemExit('QMS component manifest mismatch')
  profile=dict(line.split() for line in (src/'profile').read_text(encoding='ascii').splitlines())
  if profile!=meta.get('profile'):raise SystemExit('QMS loader profile differs from component metadata')
  if meta.get('status')!='RESTRICTED_COLD_QMS_TRIAL_PAYLOAD' or not meta.get('functional_module_built') or not meta.get('boot_loader_supplied'):
   raise SystemExit('Complete native owner/endpoint/loader payload required; frontend-only QMS is refused')
  for key,name in [('module_sha256','rk_decode_qms.ko'),('helper_sha256','boot-stage64'),('hwc_sha256','hwcomposer.rk30board.so')]:
   if sha(src/name)!=meta['profile'][key]:raise SystemExit('QMS component artifact mismatch: '+name)
  dest=out/'components/qms';shutil.copytree(src,dest)
  for name in ['rk_decode_qms.ko','boot-stage64','hwcomposer.rk30board.so']:
   files.append(sha(dest/name)+'  components/qms/'+name)
  shutil.copyfile(MODULE/'qms-sepolicy.rule',out/'sepolicy.rule')
  with (out/'config').open('a',encoding='ascii') as f:f.write('display_qms=0\n')
 (out/'files.sha256').write_bytes(('\n'.join(sorted(files))+'\n').encode())
 version=dict(line.split('=',1) for line in (out/'module.prop').read_text(encoding='utf-8').splitlines() if '=' in line)['version']
 # Include the bounded reports alongside the machine-readable results.
 for name,target in [('mali-cold-binding-040.zh-CN.md','COLD_START_COORDINATION.zh-CN.md'),
                     ('decode-controls-and-qms-cost.zh-CN.md','CONTROLS_AND_QMS_COST.zh-CN.md'),
                     ('decode-gralloc-transfer-073.zh-CN.md','GRALLOC_TRANSFER_VALIDATION.zh-CN.md'),
                     ('decode-output-delay-074.zh-CN.md','OUTPUT_DELAY_VALIDATION.zh-CN.md'),
                     ('decode-hwc-reuse-072.zh-CN.md','HWC_REUSE_VALIDATION.zh-CN.md'),
                     ('decode-hwc-overlay-075.zh-CN.md','SCALED_OVERLAY_CANDIDATE.zh-CN.md'),
                     ('decode-hevc-native-076.zh-CN.md','HEVC_NATIVE_CANDIDATE.zh-CN.md'),
                     ('decode-texture-afbc-070.zh-CN.md','TEXTURE_AFBC_VALIDATION.zh-CN.md'),
                     ('decode-lifecycle-071.zh-CN.md','LIFECYCLE_VALIDATION.zh-CN.md'),
                     ('decode-surface-route-083.zh-CN.md','SURFACE_ROUTE_CANDIDATE.zh-CN.md'),
                     ('decode-native-backpressure-084.zh-CN.md','NATIVE_BACKPRESSURE_CANDIDATE.zh-CN.md'),
                     ('decode-exo-content-vote-085.zh-CN.md','EXO_CONTENT_VOTE_CANDIDATE.zh-CN.md'),
                     ('upstream-upgrade-audit-20261018.zh-CN.md','UPSTREAM_AUDIT.zh-CN.md')]:
  report=ROOT/'docs'/name
  if report.exists():shutil.copyfile(report,out/target)
 stage_licenses(out,'decode-stack',source_ref=a.source_ref)
 package(out,w/('rk3588-decode-stack-'+version+'.zip'))
if __name__=='__main__':main()
