"""Private-input tests of real firmware/root compatibility and the native reader.

Creates disposable image files only. Never flashes, loads drivers or reboots.
"""
from pathlib import Path
import argparse,gzip,hashlib,importlib.util,json,lzma,struct,subprocess,tempfile

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('cold_prepare',ROOT/'tools/cold-boot/prepare.py')
cold=importlib.util.module_from_spec(spec);spec.loader.exec_module(cold)

PROBE=r'''
#define _POSIX_C_SOURCE 200809L
#include "cold_firmware.h"
int main(int argc,char **argv){
 if(argc!=3)return 64;
 if(!strcmp(argv[1],"sha")){
  FILE *f=fopen(argv[2],"rb");if(!f)return 65;rk_sha256 s;rk_sha256_init(&s);unsigned char b[16381];size_t n;
  while((n=fread(b,1,sizeof(b),f)))rk_sha256_update(&s,b,n);
  if(ferror(f)){fclose(f);return 66;}fclose(f);char h[65];rk_sha256_finish(&s,h);puts(h);return 0;
 }
 int mode=!strcmp(argv[1],"candidate");return rk_boot_firmware(argv[2],mode)?0:2;
}
'''

def repack(raw,plain=None,compression='gzip',mtime=0,level=9,cmd=None,change_part=None):
    head,parts,oldcmd=cold.unpack_boot(raw)
    if plain is not None:
        parts[1]=gzip.compress(plain,compresslevel=level,mtime=mtime) if compression=='gzip' else plain
        struct.pack_into('<I',head,16,len(parts[1]))
    if change_part is not None:
        i=change_part;data=bytearray(parts[i]);data[len(data)//2]^=1;parts[i]=bytes(data)
    if cmd is not None:
        encoded=cmd.encode();head[64:576]=encoded[:512].ljust(512,b'\0');head[608:1632]=encoded[512:].ljust(1024,b'\0')
    digest=hashlib.sha1()
    for part in parts:digest.update(part);digest.update(struct.pack('<I',len(part)))
    head[576:608]=digest.digest()+bytes(12)
    result=bytes(head)
    for part in parts:result+=part+bytes(cold.align(len(part),2048)-len(part))
    assert len(result)<=len(raw)
    return result.ljust(len(raw),b'\0')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--original',type=Path,required=True);p.add_argument('--package',type=Path,required=True)
    p.add_argument('--inputs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--cc',default='gcc');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    native=a.out/'firmware_probe.exe';source=a.out/'firmware_probe.c';source.write_text(PROBE,encoding='utf-8')
    subprocess.run([a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                    '-I'+str(ROOT/'magisk-mali-global/src'),str(source),'-o',str(native)],check=True)
    raw=a.original.read_bytes();_,parts,cmd=cold.unpack_boot(raw);plain=gzip.decompress(parts[1]);records,trailer=cold.cpio_records(plain)
    results=[]
    def native_check(data,expected,mode='original'):
        file=a.out/'probe-private.img';file.write_bytes(data)
        r=subprocess.run([str(native),mode,str(file)],capture_output=True)
        assert r.returncode==(0 if expected else 2),(mode,expected,r.returncode,r.stderr)
    def run(name,data,accepted,native_expected=True,service_changes=None):
        with tempfile.TemporaryDirectory(dir=a.out) as td:
            work=Path(td);image=work/'original.img';image.write_bytes(data)
            services={k:a.inputs/(v+'.rc') for k,v in [('sf','surfaceflinger'),('hwc','hwcomposer'),('allocator','allocator'),('zygote','zygote')]}
            for k,value in (service_changes or {}).items():
                f=work/(k+'.rc');f.write_text(value,encoding='utf-8');services[k]=f
            try:
                cold.prepare(image,a.package/'bin/cold_boot64',a.package,out=work/'prepared',**services)
                success=True
            except (ValueError,UnicodeError,lzma.LZMAError,struct.error):success=False
            assert success==accepted,(name,success,accepted)
            if success:
                prepared=work/'prepared';output=(prepared/'candidate.img').read_bytes()
                normalized=cold.normalize_boot_image(data)
                assert (prepared/'original.img').read_bytes()==normalized
                assert 'original_sha256 '+cold.sha(normalized)+'\n' in (prepared/'profile').read_text()
                _,outparts,_=cold.unpack_boot(output);after,_=cold.cpio_records(cold.unpack_ramdisk(outparts[1])[0])
                before,_=cold.cpio_records(cold.unpack_ramdisk(cold.unpack_boot(normalized)[1][1])[0])
                assert all(after[k][2]==v[2] for k,v in before.items()),name
                native_check(output,True,'candidate')
                native_check(normalized,True)
            native_check(data,native_expected)
            results.append({'name':name,'accepted':accepted,'native_firmware_accepted':native_expected,'passed':True})
    # Native SHA uses independent hashlib vectors, including every padding edge.
    for n in [0,1,3,55,56,63,64,65,127,128,16381,32769,1000000]:
        data=bytes((i*29+7)&255 for i in range(n));f=a.out/'sha-vector.bin';f.write_bytes(data)
        digest=subprocess.check_output([str(native),'sha',str(f)],text=True).strip()
        assert digest==hashlib.sha256(data).hexdigest(),n
    run('accepted original',raw,True)
    run('compact tool image with omitted zero tail',raw.rstrip(b'\0'),True,False)
    run('different gzip timestamp',repack(raw,plain,mtime=1712345678),True)
    run('different gzip level',repack(raw,plain,level=1),True)
    run('uncompressed newc ramdisk',repack(raw,plain,compression='cpio'),True)
    extra=cold.record('custom',b'',0o40750,300)+cold.record('custom/local-note',b'user-local data',0o100600,301)
    run('unrelated user records preserved',repack(raw,plain[:trailer]+extra+cold.record('TRAILER!!!',b'',0,302)),True)
    def replace_record(name,data,mode=None):
        output=b''
        for key,(fields,old,encoded) in records.items():
            output+=cold.record(key,data,fields[1] if mode is None else mode,fields[0]) if key==name else encoded
        return output+cold.record('TRAILER!!!',b'',0,400)
    config=records['.backup/.magisk'][1].decode()
    config='\n'.join('SHA1='+'b'*40 if line.startswith('SHA1=') else line for line in config.splitlines())+'\n'
    run('per-user Magisk backup SHA1',repack(raw,replace_record('.backup/.magisk',config.encode())),True)
    current=records['.backup/.magisk'][1];updated=current.replace(b'KEEPVERITY=false',b'KEEPVERITY=true')
    if updated==current:updated=current.replace(b'KEEPVERITY=true',b'KEEPVERITY=false')
    run('supported Magisk configuration variation',repack(raw,replace_record('.backup/.magisk',updated)),True)
    for name in records:
        if not name.endswith('.xz'):continue
        unpacked=lzma.decompress(records[name][1]);compressed=lzma.compress(unpacked,preset=0)
        run('root XZ recompression '+name,repack(raw,replace_record(name,compressed)),True)
    run('allowed logging argument',repack(raw,cmd=cmd+' loglevel=4'),True)
    run('boot argument reordering',repack(raw,cmd=' '.join(reversed(cmd.split()))),True)
    for index,label in ((0,'kernel'),(2,'second'),(4,'DTB')):
        run('reject changed '+label,repack(raw,change_part=index),False,False)
    header=bytearray(raw);header[12]^=1
    run('reject changed load address',bytes(header),False,False)
    footer=raw[:-1]+b'X';run('reject foreign signature/footer',footer,False,False)
    run('reject changed GPU init arguments',repack(raw,cmd=cmd+' '+cold.BLACKLIST),False,False)
    run('reject duplicated argument',repack(raw,cmd=cmd+' loglevel=3 loglevel=4'),False,False)
    run('reject changed critical argument',repack(raw,cmd=cmd.replace('init=/init','init=/other-init')),False,False)
    run('reject unknown boot argument',repack(raw,cmd=cmd+' unknown.option=1'),False,False)
    run('reject missing boot argument',repack(raw,cmd=cmd.replace('rootwait ','')),False,False)
    run('reject invalid root executable',repack(raw,replace_record('init',b'BAD!'+records['init'][1][4:])),False)
    run('safe root executable permission difference',repack(raw,replace_record('init',records['init'][1],0o100755)),True)
    run('reject non-executable root init',repack(raw,replace_record('init',records['init'][1],0o100600)),False)
    run('reject changed fstab',repack(raw,replace_record('fstab.rk30board',records['fstab.rk30board'][1]+b'\n')),False)
    run('reject writable root executable',repack(raw,replace_record('init',records['init'][1],0o100777)),False)
    for label,name,content in [('early init','overlay.d/custom.rc',b'on post-fs-data\n    start surfaceflinger\n'),
                               ('reserved overlay','overlay.d/rk-mali-cold.rc',b'# conflict\n'),
                               ('ambiguous root executable','overlay.d/sbin/magisk',b'unknown implementation'),
                               ('custom root init','init.rc',b'on boot\n'),
                               ('custom service','overlay.d/custom.rc',b'service custom /custom\n')]:
        added=cold.record(name,content,0o100600,500)
        run('reject '+label,repack(raw,plain[:trailer]+added+cold.record('TRAILER!!!',b'',0,501)),False)
    altered=a.inputs.joinpath('surfaceflinger.rc').read_text()+'    disabled\n'
    run('reject changed SF service',raw,False,service_changes={'sf':altered})
    run('reject changed Zygote service',raw,False,service_changes={'zygote':a.inputs.joinpath('zygote.rc').read_text()+'# comment\n    disabled\n'})
    run('allow service whitespace/comments',raw,True,service_changes={'sf':'# local comment\n'+a.inputs.joinpath('surfaceflinger.rc').read_text().replace('    ','\t')})
    report={'status':'OFFLINE_PRIVATE_INPUT_ONLY','cases':results,'native_sha256_vectors':13,
            'runtime_boot_written':False,'kernel_loaded':False,'device_acceptance':False}
    (a.out/'results-private.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS',len(results),'real-image compatibility cases and 13 native SHA vectors; no device changes')

if __name__=='__main__':main()
