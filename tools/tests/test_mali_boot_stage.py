"""Exercise the actual staging script and native firmware gate in an inert sandbox.

RK_COLD_TEST restricts native writes to regular fixture files, and this test only
invokes verify. The script's partition reference is replaced by a fixture image.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,shlex,subprocess,tarfile,tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE='/data/local/tmp/rk-cold-fixture'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('original','ndk','inputs','out'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--adb',required=True);p.add_argument('--serial',required=True)
    p.add_argument('--adb-port',default='5037');p.add_argument('--su',default='su');a=p.parse_args()
    a.out.mkdir(parents=True,exist_ok=True)
    adb=[a.adb,'-P',a.adb_port,'-s',a.serial]
    def shell(cmd,check=True):return subprocess.run(adb+['shell',shlex.quote(a.su)+' -c '+shlex.quote(cmd)],capture_output=True,text=True,encoding='utf-8',errors='replace',check=check,timeout=60)
    assert shell('test ! -e '+BASE,False).returncode==0,'Fixture already exists; inspect before retrying'
    clang=a.ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
    native=a.out/'cold_stage_fixture64'
    subprocess.run([str(clang),'--target=aarch64-linux-android32','-O2','-static','-Wall','-Wextra','-Werror',
                    '-DRK_COLD_TEST','-DRK_COLD_REAL_FIRMWARE',str(ROOT/'magisk-mali-global/src/cold_boot.c'),'-o',str(native)],check=True)
    spec=importlib.util.spec_from_file_location('cold',ROOT/'tools/cold-boot/prepare.py');cold=importlib.util.module_from_spec(spec);spec.loader.exec_module(cold)
    sha=lambda data:hashlib.sha256(data).hexdigest()
    with tempfile.TemporaryDirectory(dir=a.out) as td:
        folder=Path(td);module=folder/'module';(module/'bin').mkdir(parents=True);(module/'kernel').mkdir();(module/'contracts').mkdir()
        (module/'bin/cold_boot64').write_bytes(native.read_bytes())
        payloads={'module/kernel/rk_mali_next.ko':b'fixture KMD','module/payload/lib/egl/libGLES_mali.so':b'fixture g32',
                  'module/payload/lib64/egl/libGLES_mali.so':b'fixture g64'}
        for name,data in payloads.items():
            f=folder/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(data)
        csf=b'fixture CSF';csfhash=sha(csf)
        (module/'contracts/driver-sources.txt').write_text('arm 11 '+sha(b'fixture g32')+' https://invalid.test/a\narm64 11 '+sha(b'fixture g64')+' https://invalid.test/b\n')
        (module/'contracts/csf-source.txt').write_text('11 '+csfhash+' https://invalid.test/c\n')
        # Change only gzip metadata to prove a different user's whole-image hash
        # can stage while the exact real firmware components remain identical.
        raw=a.original.read_bytes();head,parts,_=cold.unpack_boot(raw)
        data=bytearray(raw);data[2048+cold.align(len(parts[0]),2048)+4]^=1
        # Gzip mtime is not covered by its payload CRC, but is covered by boot ID.
        _,changed,_=cold.unpack_boot(raw);changed[1]=bytes(data[2048+cold.align(len(parts[0]),2048):2048+cold.align(len(parts[0]),2048)+len(parts[1])])
        h=hashlib.sha1()
        for part in changed:h.update(part);h.update(len(part).to_bytes(4,'little'))
        data[576:608]=h.digest()+bytes(12);source=folder/'local.img';source.write_bytes(data)
        services={k:a.inputs/(v+'.rc') for k,v in [('sf','surfaceflinger'),('hwc','hwcomposer'),('allocator','allocator'),('zygote','zygote')]}
        cold.prepare(source,native,module,out=folder/'assets',**services)
        profile=(folder/'assets/profile').read_text();candidate=(folder/'assets/candidate.img').read_bytes()
        expected=sha(candidate);profilehash=sha(profile.encode())
        f=folder/'state/cache'/ (csfhash+'.bin');f.parent.mkdir(parents=True);f.write_bytes(csf)
        (module/'platform-check.sh').write_text('#!/system/bin/sh\n[ "$(cat '+BASE+'/platform-ok)" = yes ]\n',newline='\n')
        stage=(ROOT/'magisk-mali-global/boot-stage.sh').read_text().replace('/data/adb/rk3588-mali-global',BASE+'/state').replace('/data/local/tmp/rk3588-mali-cold-prepared',BASE+'/input').replace('/dev/block/mmcblk0p7',BASE+'/boot-device')
        stage=stage.replace('set -eu','set -eu\nmagisk(){ cat '+BASE+'/root-api; }',1)
        (module/'boot-stage.sh').write_text(stage,encoding='utf-8',newline='\n')
        archive=a.out/'stage-fixture-private.tar.gz'
        with tarfile.open(archive,'w:gz') as tar:
            for path in sorted(folder.rglob('*')):
                if path.is_file() and path.name not in ('local.img',):
                    info=tar.gettarinfo(str(path),path.relative_to(folder).as_posix());info.uid=info.gid=0;info.mode=0o700 if path.name=='cold_boot64' else 0o600
                    with path.open('rb') as stream:tar.addfile(info,stream)
        subprocess.run(adb+['push',str(archive),'/data/local/tmp/rk-stage-fixture-private.tar.gz'],capture_output=True,check=True,timeout=60)
    shell('mkdir -p '+BASE+' && tar -xzf /data/local/tmp/rk-stage-fixture-private.tar.gz -C '+BASE)
    results=[]
    try:
        def run(name,setup='',success=False,profile_arg=None,existing=False):
            shell('rm -rf '+BASE+'/state/boot '+BASE+'/input; mkdir '+BASE+'/input; cp '+BASE+'/assets/original.img '+BASE+'/input/original.img; cp '+BASE+'/assets/candidate.img '+BASE+'/input/candidate.img; cp '+BASE+'/assets/profile '+BASE+'/input/profile; cp '+BASE+'/assets/original.img '+BASE+'/boot-device; echo 30700 > '+BASE+'/root-api; echo yes > '+BASE+'/platform-ok')
            if setup:shell(setup)
            r=shell('sh '+BASE+'/module/boot-stage.sh '+expected+' '+(profile_arg or profilehash),False)
            assert (r.returncode==0)==success,(name,r.returncode,r.stdout,r.stderr)
            store=shell('test -e '+BASE+'/state/boot',False).returncode==0
            assert store==(success or existing),(name,'partial or missing store')
            # Native verify/staging must never modify even the fake partition.
            current=shell('sha256sum '+BASE+'/boot-device').stdout.split()[0]
            if name!='foreign current boot':assert current==sha(data),(name,'staging wrote boot')
            results.append({'name':name,'passed':True,'accepted':success})
        run('per-user image stages',success=True)
        run('foreign current boot','printf foreign > '+BASE+'/boot-device')
        run('unsupported root API','echo 30600 > '+BASE+'/root-api')
        run('wrong profile fingerprint',profile_arg='0'*64)
        run('different live graphics contract','echo no > '+BASE+'/platform-ok')
        run('corrupt original backup','printf bad > '+BASE+'/input/original.img')
        run('corrupt candidate cleans new store','printf bad > '+BASE+'/input/candidate.img')
        run('existing store preserved','mkdir '+BASE+'/state/boot; echo sentinel > '+BASE+'/state/boot/keep',existing=True)
        assert shell('cat '+BASE+'/state/boot/keep').stdout.strip()=='sentinel'
        shell('rm -rf '+BASE+'/state/boot')
        run('symlink input rejected','rm '+BASE+'/input/candidate.img; ln -s '+BASE+'/assets/candidate.img '+BASE+'/input/candidate.img')
    finally:
        shell('rm -rf '+BASE+'; rm -f /data/local/tmp/rk-stage-fixture-private.tar.gz')
    (a.out/'results-private.json').write_text(json.dumps({'status':'INERT_STAGE_FIXTURE_ONLY','cases':results,'real_boot_written':False,'kernel_loaded':False},indent=2)+'\n')
    print('PASS',len(results),'actual staging-script/native-reader cases; real boot and GPU untouched')

if __name__=='__main__':main()
