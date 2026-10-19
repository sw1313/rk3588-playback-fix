"""Prepare an exact-device PRIVATE boot overlay for Mali Global. Never flashes.

Only the known rooted AGtv boot is accepted. Kernel, second, DTB and every
existing cpio record remain unchanged. Firmware/root images are private inputs.
"""
from pathlib import Path
import argparse, gzip, hashlib, json, stat, struct

ORIGINAL = 'eabcbc53b3e56846036c8cb23fd77d246ad919aae427757e17364cb777186264'
KERNEL = 'd9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740'
BLACKLIST = 'initcall_blacklist=kbase_platform_driver_init'
def sha(b): return hashlib.sha256(b).hexdigest()
def align(n, p=4): return (n+p-1)//p*p
def require(ok, reason):
    if not ok: raise ValueError(reason)

def unpack_boot(b):
    require(b[:8] == b'ANDROID!' and len(b) == 50331648, 'Expected complete 48 MiB Android boot partition')
    page, version = struct.unpack_from('<II', b, 36)
    require(page == 2048 and version == 2, 'Unsupported boot header')
    require(struct.unpack_from('<I', b, 1632)[0] == 0, 'Unexpected recovery DTBO')
    sizes = [struct.unpack_from('<I', b, p)[0] for p in (8,16,24,1632,1648)]
    parts=[]; offset=page
    for n in sizes:
        require(offset+n <= len(b), 'Truncated component')
        parts.append(b[offset:offset+n]); offset += align(n,page)
    require(not any(b[offset:]), 'Unexpected signature/AVB/footer data; cannot repack')
    h=hashlib.sha1()
    for part in parts: h.update(part); h.update(struct.pack('<I',len(part)))
    require(b[576:608] == h.digest()+bytes(12), 'Boot component SHA1 differs')
    cmd=(b[64:576].split(b'\0')[0]+b[608:1632].split(b'\0')[0]).decode('ascii')
    return bytearray(b[:page]), parts, cmd

def cpio_records(b):
    offset=0; out={}
    while True:
        start=offset
        require(b[offset:offset+6] == b'070701', 'Only newc root ramdisk is supported')
        fields=[int(b[offset+6+i*8:offset+14+i*8],16) for i in range(13)]
        size, namesize = fields[6], fields[11]
        offset += 110; name=b[offset:offset+namesize]
        require(namesize and name[-1:] == b'\0', 'Invalid cpio name')
        name=name[:-1].decode('utf-8'); offset=align(offset+namesize)
        data=b[offset:offset+size];offset=align(offset+size)
        require(name not in out and not name.startswith('/') and '..' not in name.split('/'), 'Unsafe/duplicate cpio path')
        if name == 'TRAILER!!!':
            require(not any(b[offset:]), 'Unexpected cpio trailer data')
            return out, start
        out[name]=(fields,data,b[start:offset])

def record(name, data, mode, ino):
    n=name.encode()+b'\0'
    fields=[ino, mode,0,0,1,0,len(data),0,0,0,0,len(n),0]
    b=b'070701'+b''.join(f'{v:08x}'.encode() for v in fields)+n
    b+=bytes(align(len(b))-len(b)); b+=data
    return b+bytes(align(len(b))-len(b))

def gate(text, name, binary):
    lines=text.strip().splitlines()
    require(lines and lines[0].split()[:3] == ['service',name,binary], 'Unexpected service definition')
    require(all(not s.strip() or s.startswith((' ','\t')) for s in lines[1:]), 'Only one complete service block is allowed')
    require(not any(s.strip().split()[0] in ('override','sigstop','disabled') for s in lines[1:] if s.strip()), 'Existing service gate/override unsupported')
    return '\n'.join([lines[0],'    override','    sigstop',*lines[1:]])+'\n'

def prepare(original, loader, module, sf, hwc, allocator, zygote, out):
    raw=original.read_bytes(); require(sha(raw)==ORIGINAL,'Unrecognized root boot; no output produced')
    head,parts,cmd=unpack_boot(raw);require(sha(parts[0])==KERNEL,'Kernel identity differs')
    require('initcall_blacklist=' not in cmd,'Existing blacklist is unsupported')
    require(loader.read_bytes()[:5]==b'\x7fELF\x02','Expected ARM64 native keeper')
    require(struct.unpack_from('<H',loader.read_bytes(),18)[0]==183,'Keeper is not AArch64')
    require(not out.exists(),'Use a fresh private output directory')
    values={
        'original_sha256':ORIGINAL,'original_bytes':str(len(raw)),
        'kmd_sha256':sha((module/'kernel/rk_mali_next.ko').read_bytes()),
        'csf_sha256':(module/'contracts/csf-source.txt').read_text().split()[1],
    }
    drivers={r.split()[0]:r.split()[2] for r in (module/'contracts/driver-sources.txt').read_text().splitlines()}
    values.update(gles32_sha256=drivers['arm'],gles64_sha256=drivers['arm64'])
    sf_gate=gate(sf.read_text(), 'surfaceflinger','/system/bin/surfaceflinger')
    hwc_gate=gate(hwc.read_text(), 'vendor.hwcomposer-2-1','/vendor/bin/hw/android.hardware.graphics.composer@2.1-service')
    allocator_gate=gate(allocator.read_text(),'vendor.gralloc-4-0','/vendor/bin/hw/android.hardware.graphics.allocator@4.0-service')
    # Never override Zygote: doing so also discards Magisk's onrestart patch,
    # and pre-exec SIGSTOP interferes with external Zygisk injection monitors.
    require('service zygote /system/bin/app_process64' in zygote.read_text() and 'service zygote_secondary /system/bin/app_process32' in zygote.read_text(),'Expected zygote64_32 definitions')
    recipe={'profile':values,'keeper_sha256':sha(loader.read_bytes()),'sf':sf_gate,'hwc':hwc_gate,'allocator':allocator_gate,'zygote_definition_sha256':sha(zygote.read_bytes()),'magisk_stage_api':30700,'barrier':'after idempotent Magisk post-fs-data, before original zygote-start actions'}
    identity=sha(json.dumps(recipe,sort_keys=True,separators=(',',':')).encode())
    profile='profile_id '+identity+'\n'+''.join(k+' '+v+'\n' for k,v in values.items())
    rc=allocator_gate+'\n'+hwc_gate+'\n'+sf_gate+'''\n# Magisk appends custom rc BEFORE its own post-fs-data action. Dispatch its
# idempotent stage here first; the later stock call is a verified no-op on 30700.
# init exec keeps handling control messages while its action queue waits.
on post-fs-data
    exec u:r:magisk:s0 root root -- ${MAGISKTMP}/rk-mali-cold preflight ${MAGISKTMP}/rk-mali-cold.profile
    exec u:r:magisk:s0 root root -- ${MAGISKTMP}/magisk --post-fs-data
    start vendor.gralloc-4-0
    start vendor.hwcomposer-2-1
    start surfaceflinger
    exec_background u:r:magisk:s0 root root -- ${MAGISKTMP}/rk-mali-cold stage ${MAGISKTMP}/rk-mali-cold.profile
    exec u:r:magisk:s0 root root -- ${MAGISKTMP}/rk-mali-cold barrier ${MAGISKTMP}/rk-mali-cold.profile
'''
    plain=gzip.decompress(parts[1]);records,trailer=cpio_records(plain)
    for name in ('init','.backup/.magisk','.backup/init.xz','overlay.d/sbin/magisk.xz'):
        require(name in records,'Root component missing: '+name)
    additions={'overlay.d/rk-mali-cold.rc':(rc.encode(),0o100644),
               'overlay.d/sbin/rk-mali-cold':(loader.read_bytes(),0o100700),
               'overlay.d/sbin/rk-mali-cold.profile':(profile.encode(),0o100600)}
    require(not set(additions)&set(records),'Cold overlay already exists')
    extra=b'';ino=max(v[0][0] for v in records.values())+1
    for name in ('overlay.d','overlay.d/sbin'):
        if name not in records: extra+=record(name,b'',0o40750,ino);ino+=1
    for name,(data,mode) in additions.items(): extra+=record(name,data,mode,ino);ino+=1
    new_plain=plain[:trailer]+extra+record('TRAILER!!!',b'',0,ino)
    after,_=cpio_records(new_plain)
    require(all(after[k][2]==v[2] for k,v in records.items()),'Original root ramdisk record changed')
    parts[1]=gzip.compress(new_plain,compresslevel=9,mtime=0)
    struct.pack_into('<I',head,16,len(parts[1]))
    cmd+=' '+BLACKLIST; encoded=cmd.encode('ascii');require(len(encoded)<1536,'Cmdline overflow')
    head[64:576]=encoded[:512].ljust(512,b'\0');head[608:1632]=encoded[512:].ljust(1024,b'\0')
    digest=hashlib.sha1()
    for part in parts: digest.update(part);digest.update(struct.pack('<I',len(part)))
    head[576:608]=digest.digest()+bytes(12)
    candidate=bytes(head)
    for part in parts: candidate+=part+bytes(align(len(part),2048)-len(part))
    require(len(candidate)<=len(raw),'Boot partition capacity exceeded')
    candidate=candidate.ljust(len(raw),b'\0')
    _,check,check_cmd=unpack_boot(candidate)
    _,before,_=unpack_boot(raw)
    require(all(check[i]==before[i] for i in (0,2,3,4)) and check_cmd.endswith(BLACKLIST),'Roundtrip component failure')
    out.mkdir(parents=True)
    (out/'original.img').write_bytes(raw);(out/'candidate.img').write_bytes(candidate)
    (out/'profile').write_text(profile,encoding='ascii',newline='\n')
    (out/'image.sha256').write_text(sha(candidate)+'\n',encoding='ascii')
    (out/'gates.rc').write_text(rc,encoding='ascii',newline='\n')
    (out/'PREPARED.json').write_text(json.dumps({'status':'UNVALIDATED_PRIVATE_CANDIDATE','profile_id':identity,'candidate_sha256':sha(candidate),'original_sha256':ORIGINAL,'unchanged_components':['kernel','second','recovery_dtbo','dtb'],'original_cpio_records_preserved':len(records),'boot_write_performed':False},indent=2)+'\n')
    return identity

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('original','loader','module','sf','hwc','allocator','zygote','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();print(prepare(**vars(a)))
