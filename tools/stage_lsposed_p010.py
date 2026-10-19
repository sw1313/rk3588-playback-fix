"""Stage a source-built, hash-scoped LSPosed client payload; never copy APK code."""
import argparse,hashlib,json,shutil,subprocess,zipfile,re,struct
from pathlib import Path
PINNED={
 'libavcodec.so':'d5b74992d3fa6b00ea1c7a46c2c9e025c973a2aeb57a571cbf491b9b15fd9854',
 'libavutil.so':'2d4c775fcc8c14aba03dd4a202bea126dc948d9b0f61d57ec2ba740f1ab34cc6',
 'libswresample.so':'213baa09e59a69c9bc69f1326114d80f858d92b0ec344afb9ae54dc5e79e95c9',
 'libmpv.so':'85407059a0f23325392db916175ca322bddc501b076ee8e89888ee11c51d187b'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def sanitize_configuration(library):
 """Redact build paths only inside the readonly FFmpeg configuration string.

 Equal-length replacement preserves every ELF offset and executable byte.
 Unknown layouts fail closed; this is not a general binary string patcher.
 """
 b=library.read_bytes()
 assert b[:6]==b'\x7fELF\x02\x01','Little-endian ELF64 required'
 marker=b'--target-os=android';assert b.count(marker)==1
 start=b.index(marker);assert b[start-1]==0
 end=b.index(b'\0',start)
 assert b'--enable-mediacodec' in b[start:end]
 shoff=struct.unpack_from('<Q',b,40)[0]
 shsize,shcount=struct.unpack_from('<HH',b,58)
 assert shsize==64 and 0<shcount<65536
 sections=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*shsize) for i in range(shcount)]
 assert any(s[1]!=8 and s[2]&2 and not s[2]&5 and s[4]<=start and end<s[4]+s[5] for s in sections),'Configuration must be in readonly non-executable storage'
 value=b[start:end]
 pattern=rb'(?:[A-Za-z]:[/\\]|/home/|/Users/)[^ \t\x00\x27\x22]+'
 value,count=re.subn(pattern,lambda m:b'/b'.ljust(len(m[0]),b'_'),value)
 assert len(value)==end-start
 result=b[:start]+value+b[end:]
 for s in sections:
  if s[2]&4:assert result[s[4]:s[4]+s[5]]==b[s[4]:s[4]+s[5]]
 library.write_bytes(result)
 return {'configuration_paths_redacted':count,'executable_bytes_unchanged':True,'ELF_offsets_unchanged':True,'configuration_string_offset':start,'configuration_string_length':end-start}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['library','original-arm64-apk','source-archive','source-archive-sha256','llvm-bin','framework-adapter','output']:p.add_argument('--'+name,required=True)
 a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False)
 archive=Path(a.source_archive);assert sha(archive)==a.source_archive_sha256,'Pinned public source archive differs'
 with zipfile.ZipFile(a.original_arm64_apk) as z:
  for name,digest in PINNED.items():assert hashlib.sha256(z.read('lib/arm64-v8a/'+name)).hexdigest()==digest,'Original player ABI differs: '+name
 lib=out/'libavcodec.so';shutil.copyfile(a.library,lib)
 ext='.exe' if __import__('os').name=='nt' else ''
 subprocess.run([str(Path(a.llvm_bin)/('llvm-strip'+ext)),'--strip-unneeded',str(lib)],check=True)
 sanitized=sanitize_configuration(lib)
 assert lib.stat().st_size<32*1024*1024
 contract={'schema':1,'status':'UNVALIDATED_CLIENT_CANDIDATE','target_package':'com.hush.yamby','api':32,'abi':'arm64-v8a','upstream':'https://github.com/FFmpeg/FFmpeg/tree/bf4d5037b4','source_archive_sha256':sha(archive),'payload_sha256':sha(lib),'original_libraries':PINNED,'scope':'HEVC known Main10 RK MediaCodec copy-back P010; no Surface override or bit-depth reduction','original_APK_untouched':True,'rollback':'Disable hw_main10_copy or LSPosed module and restart the app/reboot','hardware_acceptance':False}
 adapter=Path(a.framework_adapter)
 assert adapter.read_bytes()[:5]==b'\x7fELF\x02','ARM64 framework adapter required'
 contract['requires_system_property']='debug.rkcodec2.p010_pool=1'
 contract['build_metadata_sanitization']=sanitized
 contract['framework_libraries']={'/system/lib64/libsfplugin_ccodec.so':'f9e01d5ff014ab42a40ddaafee6f3df8122502dd1f0641de58ae75b1b96610dd','/system/lib64/libcp10.so':sha(adapter)}
 (out/'contract.json').write_text(json.dumps(contract,indent=2)+'\n',encoding='utf-8')
 shutil.copyfile(archive,out/'ffmpeg-source.tar.gz')
 root=Path(__file__).resolve().parents[1]
 shutil.copyfile(root/'lsposed/patches/ffmpeg-rk-hevc-p010-copy.patch',out/'ffmpeg-rk-hevc-p010-copy.patch')
 shutil.copyfile(root/'LICENSES/dav1d/COPYING',out/'dav1d-COPYING')
 print('STAGED_SOURCE_BUILT_P010',sha(lib),lib.stat().st_size)
if __name__=='__main__':main()
