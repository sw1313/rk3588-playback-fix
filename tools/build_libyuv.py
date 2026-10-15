"""Build an API32 libyuv upgrade, preserving the firmware's removed APIs.

The two stock inputs are private link/ABI checks only and never enter the ZIP.
"""
import argparse,hashlib,json,os,shlex,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];MODULE=ROOT/'magisk-libyuv'
NEW='58d817ed81e8fe8b06a58576d639cd56b19526d3'
OLD='715a8b0584d482569ed39ee95291cfa9061c6aaf'
def run(*args):return subprocess.check_output([str(a) for a in args],text=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['ndk','stock-libyuv','stock-libjpeg','jpeg-headers']:p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--work',type=Path,required=True);a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True)
 if sha(a.stock_libyuv)!='22358071bb74ad19fbe4b2f5fcba4d4130f680deddf7df02e635674b9824233e':raise SystemExit('Unknown original libyuv ABI')
 if sha(a.stock_libjpeg)!='5d57b8343b6a56af65b6682b0e4ca8c63defb1c1ea3da1ec77eb2a445b7f0aa4':raise SystemExit('Unknown JPEG link input')
 n=a.ndk.resolve();host='windows-x86_64' if os.name=='nt' else 'darwin-x86_64' if os.uname().sysname=='Darwin' else 'linux-x86_64';ext='.exe' if os.name=='nt' else '';llvm=n/'toolchains/llvm/prebuilt'/host/'bin'
 def symbols(file,archive=False):
  args=[llvm/('llvm-nm'+ext),'--extern-only','--defined-only']+([] if archive else ['-D'])
  return {x.split()[-1] for x in run(*args,file).splitlines() if len(x.split())>=3 and x.split()[-2] in ['T','D','B','R','W','V']}
 for label,url,branch,rev,sub in [('new','https://chromium.googlesource.com/libyuv/libyuv','main',NEW,''),('old','https://android.googlesource.com/platform/external/libyuv','android-12.1.0_r27',OLD,'files')]:
  src=w/(label+'-source')
  if not src.exists():run('git','clone','--no-checkout','--depth','1','--branch',branch,url,src)
  if run('git','-C',src,'rev-parse','HEAD').strip()!=rev:run('git','-C',src,'fetch','--depth','1','origin',rev)
  if run('git','-C',src,'diff','--name-only').strip():raise SystemExit('Modified upstream source')
  run('git','-C',src,'checkout','--detach',rev)
  maps=' '.join('-ffile-prefix-map='+v.as_posix()+'='+k for v,k in [(w,'build'),(ROOT,'module'),(a.jpeg_headers.resolve(),'jpeg-sdk')])
  run('cmake','-S',src/sub,'-B',w/label,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(n/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-32','-DANDROID_STL=c++_static','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DJPEG_INCLUDE_DIR='+str(a.jpeg_headers.resolve()),'-DJPEG_LIBRARY='+str(a.stock_libjpeg.resolve()),'-DCMAKE_C_FLAGS='+maps,'-DCMAKE_CXX_FLAGS='+maps)
  run('cmake','--build',w/label,'--target','yuv_shared' if label=='new' else 'yuv','--parallel','6')
 stock=symbols(a.stock_libyuv);latest=symbols(w/'new/libyuv.so');legacy=symbols(w/'old/libyuv.a',True);missing=stock-latest
 if missing-legacy:raise SystemExit('Some old APIs cannot be preserved')
 declared=set((MODULE/'contracts/symbols.txt').read_text(encoding='utf-8').splitlines())
 if declared!=stock:raise SystemExit('Original symbol contract differs')
 rename=w/'legacy-symbols.txt';rename.write_bytes(''.join(s+' rklegacy_'+s+'\n' for s in sorted(legacy-missing)).encode())
 archive=w/'legacy-isolated.a';run(llvm/('llvm-objcopy'+ext),'--redefine-syms='+str(rename),w/'old/libyuv.a',archive)
 script=w/'exports.map';script.write_bytes(b'{ global: *; local: rklegacy_*; };\n')
 cmds=run('ninja','-C',w/'new','-t','commands','yuv_shared').splitlines();line=next(c for c in reversed(cmds) if '-Wl,-soname,libyuv.so' in c)
 if os.name=='nt':line=line.split('&& ',1)[1].rsplit(' &&',1)[0]
 args=[x.strip('"') for x in shlex.split(line,posix=os.name!='nt')]
 payload=w/'package'
 if payload.exists():raise SystemExit('Use a fresh build work directory')
 shutil.copytree(MODULE,payload,ignore=shutil.ignore_patterns('system','bin','checksums.sha256','status.txt','disable'))
 dest=payload/'system/lib64/libyuv.so';dest.parent.mkdir(parents=True);args[args.index('-o')+1]=str(dest)
 args+=['-Wl,--whole-archive',str(archive),'-Wl,--no-whole-archive','-Wl,--version-script='+str(script)]
 subprocess.run(args,cwd=w/'new',check=True);run(llvm/('llvm-strip'+ext),'--strip-unneeded',dest)
 if stock-symbols(dest):raise SystemExit('Final symbol contract failed')
 (payload/'bin').mkdir();run(llvm/('clang++'+ext),'--target=aarch64-linux-android32','-O2','-static-libstdc++','-fPIE','-pie','-s','-I'+str(w/'new-source/include'),'-ffile-prefix-map='+ROOT.as_posix()+'=module',MODULE/'src/quality_probe.cpp','-ldl','-o',payload/'bin/quality_probe64')
 (payload/'UPSTREAM.json').write_text(json.dumps(dict(libyuv=NEW,android12_compat=OLD,version=1974,removed_apis_preserved=sorted(missing),all_original_symbols_preserved=True),indent=2))
 package(payload,w/'rk3588-libyuv-upgrade-0.1.0-yuv1974.zip')
def package(payload,output):
 files=sorted(f for f in payload.rglob('*') if f.is_file() and f.name!='checksums.sha256')
 for f in files:
  if f.suffix in ['.sh','.md','.txt','.prop','.sha256','.json','.cpp']:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (payload/'checksums.sha256').write_bytes(''.join(sha(f)+'  '+f.relative_to(payload).as_posix()+'\n' for f in files).encode())
 with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(payload.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(payload).as_posix())
 print('ZIP',output,'SHA256',sha(output))
if __name__=='__main__':main()
