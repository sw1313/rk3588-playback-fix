"""Rebuild both GPL Mali modules from the frozen source and pinned ABI inputs.

Needs pristine Linux 5.10.66 headers and a host-built matching scripts/mod/modpost.
No original kernel image, firmware, or boot partition is copied into the output.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'mali-kernel'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['ndk','kernel-source','modpost','work']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--host-tools',type=Path,help='Optional directory containing host modpost runtime/tools')
 a=p.parse_args();w=a.work.resolve()
 if w.exists():raise SystemExit('Use a fresh output directory')
 w.mkdir(parents=True);prepared=w/'kbuild';shutil.copytree(SOURCE/'prepared',prepared)
 ndk=a.ndk.resolve();ks=a.kernel_source.resolve();host='windows-x86_64' if os.name=='nt' else 'linux-x86_64';ext='.exe' if os.name=='nt' else ''
 llvm=ndk/'toolchains/llvm/prebuilt'/host/'bin'
 env=dict(os.environ)
 if a.host_tools:env['PATH']=str(a.host_tools.resolve())+os.pathsep+env.get('PATH','')
 paths={'NDK':ndk,'KERNEL_SOURCE':ks,'KERNEL_BUILD':prepared,'DRIVER':SOURCE/'driver','COMPAT':SOURCE/'compat'}
 def mapped(v):
  for key,value in paths.items():v=v.replace('@'+key+'@',value.as_posix())
  assert '@' not in v,v
  return v
 def args(name):
  values=[mapped(v) for v in json.loads((SOURCE/name).read_text())]
  values=[v for v in values if not v.startswith(('-fmacro-prefix-map=','-ffile-prefix-map='))]
  for local,public in [(ROOT,'/src'),(ndk,'/ndk'),(ks,'/kernel'),(prepared,'/kbuild'),(w,'/obj')]:
   for spelling in {str(local),local.as_posix()}:
    values+=['-ffile-prefix-map='+spelling+'='+public,'-fmacro-prefix-map='+spelling+'='+public]
  return [llvm/('clang'+ext),*values]
 def run(command):
  r=subprocess.run([str(x).replace('\\','/') for x in command],cwd=prepared,capture_output=True,timeout=120,env=env)
  if r.returncode:raise RuntimeError('Tool exit '+str(r.returncode)+': '+(r.stdout+r.stderr).decode('utf-8','replace')[-6000:])
  return r.stdout.decode('utf-8','replace')
 def compile(name,src,dest,base,extra=()):
  run(args(name)+list(extra)+['-DKBUILD_BASENAME="'+base+'"','-c',src,'-o',dest])
 def finish(name,obj,units,argfile,extra=()):
  (w/(name+'.mod')).write_text(' '.join(x.as_posix() for x in units)+'\n',newline='\n')
  for unit in units:unit.with_name('.'+unit.name+'.cmd').write_text('source_frozen := '+(SOURCE/'driver').as_posix()+'\n',newline='\n')
  run([a.modpost.resolve(),'-m','-e','-i',SOURCE/'image.Module.symvers','-o',w/(name+'.Module.symvers'),obj])
  modc=w/(name+'.mod.c');compile(argfile,modc,w/(name+'.mod.o'),name+'.mod',extra)
  ko=w/(name+'.ko');run([llvm/('ld.lld'+ext),'-r','-EL','-maarch64elf','-z','norelro','--build-id=sha1','-T',prepared/'module.lds','-o',ko,obj,w/(name+'.mod.o')])
  expected={l.split()[1]:int(l.split()[0],16) for l in (SOURCE/'image.Module.symvers').read_text().splitlines()}
  versions={n:int(c,16) for c,n in re.findall(r'\{ (0x[0-9a-f]+), "([^\"]+)" \}',modc.read_text())}
  assert versions and all(expected[n]==c for n,c in versions.items())
  return {'sha256':sha(ko),'canonical_import_versions':len(versions)}
 objects=[]
 for item in json.loads((SOURCE/'objects.json').read_text()):
  obj=w/'objects'/item;obj.parent.mkdir(parents=True,exist_ok=True);src=SOURCE/'driver'/Path(item).with_suffix('.c')
  compile('candidate-args.json',src,obj,Path(item).stem,['-I'+src.parent.as_posix()]);objects.append(obj)
 partial=w/'partial.o';run([llvm/('ld.lld'+ext),'-r','-o',partial,*objects])
 obj=w/'rk_mali_next.o';run([llvm/('ld.lld'+ext),'-r','-T',SOURCE/'export-crcs.lds','-o',obj,partial])
 results={'candidate':finish('rk_mali_next',obj,objects,'candidate-args.json')}
 name='rk_mali_resource_guard';obj=w/(name+'.o')
 extra=['-I'+(SOURCE/'guard').as_posix(),'-DKBUILD_MODNAME="'+name+'"','-D__KBUILD_MODNAME=kmod_'+name]
 # Guard has no Mali configuration or exported namespace substitutions.
 guard_args=[x for x in json.loads((SOURCE/'guard-args.json').read_text()) if not x.startswith(('-DMALI_','-DKBUILD_','-D__KBUILD_'))]
 temporary=SOURCE/'guard-args.filtered.json'
 try:
  temporary.write_text(json.dumps(guard_args))
  compile('guard-args.filtered.json',SOURCE/'guard'/name.replace(name,name+'.c'),obj,name,extra)
  results['guard']=finish(name,obj,[obj],'guard-args.filtered.json',extra)
 finally:temporary.unlink(missing_ok=True)
 for item in ['rk_mali_next.ko','rk_mali_resource_guard.ko']:
  raw=(w/item).read_bytes()
  for marker in [str(ROOT).encode(),ROOT.as_posix().encode(),str(ndk).encode(),ndk.as_posix().encode()]:
   assert marker not in raw,(item,'private build path')
 (w/'BUILD.json').write_text(json.dumps(results,indent=2))
 print(json.dumps(results,indent=2))

if __name__=='__main__':main()
