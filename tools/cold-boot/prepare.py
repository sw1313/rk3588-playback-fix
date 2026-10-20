"""Prepare an exact-device PRIVATE boot overlay for Mali Global. Never flashes.

Accept known firmware components with a supported local Magisk ramdisk, rather
than one person's complete boot digest. Existing cpio records are preserved.
Firmware/root images and per-user original/candidate digests remain private.
"""
from pathlib import Path
import argparse, gzip, hashlib, json, lzma, re, stat, struct, zlib

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = json.loads((ROOT/'magisk-mali-global/contracts/boot-firmware.json').read_text(encoding='utf-8'))
BLACKLIST = 'initcall_blacklist=kbase_platform_driver_init'
def sha(b): return hashlib.sha256(b).hexdigest()
def align(n, p=4): return (n+p-1)//p*p
def require(ok, reason):
    if not ok: raise ValueError(reason)

def normalize_boot_image(raw):
    # Image-patching tools may omit the unused zero-filled partition tail.
    # All component hashes and the boot ID are still checked after padding;
    # staging must also match the full live partition before accepting it.
    require(2048<=len(raw)<=CONTRACT['partition_bytes'],'Unsupported boot image capacity')
    return raw.ljust(CONTRACT['partition_bytes'],b'\0')

def normalize_service(text):
    return '\n'.join(' '.join(line.split()) for line in text.splitlines()
                     if line.strip() and not line.lstrip().startswith('#'))+'\n'

def validate_firmware(raw, candidate=False):
    head,parts,cmd=unpack_boot(raw)
    fixed=bytearray(head)
    for start,end in ((16,20),(64,576),(576,608),(608,1632)):
        fixed[start:end]=bytes(end-start)
    require(sha(fixed)==CONTRACT['fixed_header_sha256'],'Firmware boot header differs')
    for index,expected in CONTRACT['parts'].items():
        part=parts[int(index)]
        require(len(part)==expected['bytes'] and sha(part)==expected['sha256'],
                'Firmware component differs: '+index)
    tokens=cmd.split(); base=CONTRACT['cmdline'].split()
    require(len(tokens)==len(set(tokens)),'Duplicate boot argument')
    keys=[x.split('=',1)[0] for x in tokens]
    require(len(keys)==len(set(keys)),'Conflicting boot arguments')
    require(set(base)<=set(tokens),'Required firmware boot argument differs')
    extras=set(tokens)-set(base)
    if candidate:
        require(BLACKLIST in extras,'Candidate GPU init blacklist missing')
        extras.remove(BLACKLIST)
    require(extras<=set(CONTRACT['allowed_extra_cmdline']),'Unsupported boot argument')
    return head,parts,cmd

def unpack_ramdisk(data):
    if data.startswith(b'070701'):
        require(len(data)<=64*1024*1024,'Ramdisk exceeds limit')
        return data,'cpio'
    require(data.startswith(b'\x1f\x8b'),'Unsupported ramdisk compression; expected gzip or newc')
    decoder=zlib.decompressobj(16+zlib.MAX_WBITS)
    plain=decoder.decompress(data,64*1024*1024+1)
    require(len(plain)<=64*1024*1024 and decoder.eof and not decoder.unused_data,
            'Invalid or oversized gzip ramdisk')
    return plain,'gzip'

def validate_root(records):
    for name,expected in CONTRACT['ramdisk_firmware_records'].items():
        require(name in records,'Ramdisk firmware component missing: '+name)
        fields,data,_=records[name]
        require(sha(data)==expected['sha256'] and fields[1]==expected['mode'] and fields[2:4]==[0,0],
                'Ramdisk firmware component differs: '+name)
    for name in ('overlay.d','overlay.d/sbin','system','system/etc','system/etc/ramdisk','.backup'):
        require(name in records and stat.S_ISDIR(records[name][0][1]) and records[name][0][2:4]==[0,0]
                and not records[name][0][1]&0o022,
                'Required root directory differs: '+name)
    for name,expected in CONTRACT['root_records'].items():
        require(name in records,'Root component missing: '+name)
        fields,data,_=records[name]
        mode=fields[1]
        require(stat.S_ISREG(mode) and fields[2:4]==[0,0] and not mode&0o7022
                and mode&0o400 and (not expected['mode']&0o100 or mode&0o100),
                'Root component ownership/mode differs: '+name)
        if expected['xz']:
            dec=lzma.LZMADecompressor()
            data=dec.decompress(data,max_length=64*1024*1024+1)
            require(dec.eof and not dec.unused_data and len(data)<=64*1024*1024,
                    'Invalid root payload: '+name)
        require(sha(data)==expected['sha256'],'Unsupported root implementation: '+name)
    require('.backup/.magisk' in records,'Magisk configuration missing')
    fields,data,_=records['.backup/.magisk']
    require(stat.S_ISREG(fields[1]) and fields[2:4]==[0,0] and not fields[1]&0o022,
            'Untrusted Magisk configuration')
    config={}
    for line in data.decode('ascii').splitlines():
        require('=' in line,'Malformed Magisk configuration')
        key,value=line.split('=',1)
        require(key not in config and key in ('KEEPVERITY','KEEPFORCEENCRYPT','RECOVERYMODE',
                'VENDORBOOT','PREINITDEVICE','SHA1'),'Unknown/duplicate Magisk setting')
        config[key]=value
    for key in ('KEEPVERITY','KEEPFORCEENCRYPT','RECOVERYMODE','VENDORBOOT'):
        require(config.get(key) in ('true','false'),'Invalid Magisk boolean: '+key)
    require(config['RECOVERYMODE']=='false' and config['VENDORBOOT']=='false',
            'Unsupported recovery/vendor_boot root layout')
    require(re.fullmatch(r'[0-9a-f]{40}',config.get('SHA1','')) is not None,
            'Invalid original Magisk SHA1')
    require(re.fullmatch(r'[A-Za-z0-9_.-]*',config.get('PREINITDEVICE','')) is not None,
            'Unsupported Magisk preinit device')
    for name,(fields,data,_) in records.items():
        require(not name.startswith(('overlay.d/sbin/rk-mali-cold','overlay.d/rk-mali-cold')),
                'Existing cold-binding overlay; restore it before preparing a new base')
        require(name not in ('overlay.d/sbin/magisk','overlay.d/sbin/magisk32','overlay.d/sbin/magisk64',
                             'overlay.d/sbin/magiskinit','overlay.d/sbin/init-ld','overlay.d/sbin/stub'),
                'Overlay would shadow a supported root executable: '+name)
        require(not (name.endswith('.rc') and not name.startswith('overlay.d/')),
                'Custom root init script requires separate adaptation: '+name)
        require(not (name.split('/')[0].startswith('fstab') and name not in CONTRACT['ramdisk_firmware_records']),
                'Unknown firmware mount table: '+name)
        if name.startswith('overlay.d/') and name.endswith('.rc'):
            # Unknown early actions/imports can bypass the first-binding barrier.
            text=data.decode('utf-8')
            require(not re.search(r'(?m)^\s*(import|service)\b',text),
                    'Custom init services/imports require separate adaptation: '+name)
            require(not re.search(r'(?m)^\s*on\s+(early-init|init|late-init|fs|post-fs|post-fs-data|boot)\b',text),
                    'Custom early init action requires separate adaptation: '+name)
            require(not re.search(r'(zygote|surfaceflinger|hwcomposer|gralloc|magisk|rk-mali|/sys/.*gpu)',text),
                    'Conflicting graphics/root init hook: '+name)

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
        require(offset+110<=len(b),'Truncated cpio header')
        require(b[offset:offset+6] == b'070701', 'Only newc root ramdisk is supported')
        fields=[int(b[offset+6+i*8:offset+14+i*8],16) for i in range(13)]
        size, namesize = fields[6], fields[11]
        offset += 110; name=b[offset:offset+namesize]
        require(namesize and len(name)==namesize and name[-1:] == b'\0', 'Invalid cpio name')
        name=name[:-1].decode('utf-8'); offset=align(offset+namesize)
        require(offset+size<=len(b),'Truncated cpio payload')
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
    lines=[line for line in text.strip().splitlines() if line.strip() and not line.lstrip().startswith('#')]
    require(lines and lines[0].split()[:3] == ['service',name,binary], 'Unexpected service definition')
    require(all(not s.strip() or s.startswith((' ','\t')) for s in lines[1:]), 'Only one complete service block is allowed')
    require(not any(s.strip().split()[0] in ('override','sigstop','disabled') for s in lines[1:] if s.strip()), 'Existing service gate/override unsupported')
    return '\n'.join([lines[0],'    override','    sigstop',*lines[1:]])+'\n'

def prepare(original, loader, module, sf, hwc, allocator, zygote, out):
    raw=normalize_boot_image(original.read_bytes())
    head,parts,cmd=validate_firmware(raw)
    for key,path in [('sf',sf),('hwc',hwc),('allocator',allocator),('zygote',zygote)]:
        require(sha(normalize_service(path.read_text(encoding='utf-8')).encode())==CONTRACT['services_sha256'][key],
                'Unsupported graphics/zygote service definition: '+key)
    require('initcall_blacklist=' not in cmd,'Existing blacklist is unsupported')
    require(loader.read_bytes()[:5]==b'\x7fELF\x02','Expected ARM64 native keeper')
    require(struct.unpack_from('<H',loader.read_bytes(),18)[0]==183,'Keeper is not AArch64')
    require(loader.read_bytes()==(module/'bin/cold_boot64').read_bytes(),
            'Keeper must belong to the selected module package')
    require(not out.exists(),'Use a fresh private output directory')
    values={
        'original_sha256':sha(raw),'original_bytes':str(len(raw)),
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
    zygote_normalized=normalize_service(zygote.read_text(encoding='utf-8'))
    require('service zygote /system/bin/app_process64' in zygote_normalized and 'service zygote_secondary /system/bin/app_process32' in zygote_normalized,'Expected zygote64_32 definitions')
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
    plain,compression=unpack_ramdisk(parts[1]);records,trailer=cpio_records(plain)
    validate_root(records)
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
    parts[1]=gzip.compress(new_plain,compresslevel=9,mtime=0) if compression=='gzip' else new_plain
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
    _,check,check_cmd=validate_firmware(candidate,candidate=True)
    _,before,_=unpack_boot(raw)
    require(all(check[i]==before[i] for i in (0,2,3,4)) and check_cmd.endswith(BLACKLIST),'Roundtrip component failure')
    out.mkdir(parents=True)
    (out/'original.img').write_bytes(raw);(out/'candidate.img').write_bytes(candidate)
    (out/'profile').write_text(profile,encoding='ascii',newline='\n')
    (out/'image.sha256').write_text(sha(candidate)+'\n',encoding='ascii')
    (out/'gates.rc').write_text(rc,encoding='ascii',newline='\n')
    (out/'PREPARED.json').write_text(json.dumps({'status':'UNVALIDATED_PRIVATE_CANDIDATE','profile_id':identity,'candidate_sha256':sha(candidate),'original_sha256':sha(raw),'firmware_contract_sha256':sha(json.dumps(CONTRACT,sort_keys=True).encode()),'root_api':CONTRACT['root_api'],'ramdisk_compression':compression,'unchanged_components':['kernel','second','recovery_dtbo','dtb'],'original_cpio_records_preserved':len(records),'boot_write_performed':False},indent=2)+'\n')
    return identity

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('original','loader','module','sf','hwc','allocator','zygote','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();print(prepare(**vars(a)))
