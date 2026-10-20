"""Exercise version-neutral root interfaces/layouts with disposable firmware inputs.

APK inputs and firmware stay private. This never changes a real root/boot.
"""
from pathlib import Path
import argparse,gzip,hashlib,importlib.util,json,lzma,struct,tempfile,zipfile

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('cold',ROOT/'tools/cold-boot/prepare.py')
cold=importlib.util.module_from_spec(spec);spec.loader.exec_module(cold)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('original','package','inputs','apks','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    raw=a.original.read_bytes();_,parts,_=cold.unpack_boot(raw)
    records,_=cold.cpio_records(cold.unpack_ramdisk(parts[1])[0])
    samples=json.loads((ROOT/'tools/tests/data/magisk-upstream-fixtures.json').read_text(encoding='utf-8'))['profiles']
    root_names=set().union(*(p['root_records'] for p in samples))
    results=[]
    for profile in samples:
        apk=a.apks/('Magisk-v'+profile['version']+'.apk')
        assert cold.sha(apk.read_bytes())==profile['apk_sha256']
        base={k:v for k,v in records.items() if k not in root_names}
        with zipfile.ZipFile(apk) as z:
            for name,entry in profile['root_records'].items():
                data=z.read(entry['apk_entry']);assert cold.sha(data)==entry['sha256']
                encoded=cold.record(name,lzma.compress(data) if entry['xz'] else data,entry['mode'],900+len(base))
                rec,_=cold.cpio_records(encoded+cold.record('TRAILER!!!',b'',0,9999))
                base.update(rec)
        if profile['config_vendorboot_optional']:
            entry=base['.backup/.magisk'];config=b'\n'.join(x for x in entry[1].splitlines() if not x.startswith(b'VENDORBOOT='))+b'\n'
            rec,_=cold.cpio_records(cold.record('.backup/.magisk',config,entry[0][1],9010)+cold.record('TRAILER!!!',b'',0,9999));base.update(rec)
        assert cold.validate_root(base)['backend']=='magisk-overlay-postfs'
        with tempfile.TemporaryDirectory(dir=a.out) as td:
            work=Path(td);head,newparts,_=cold.unpack_boot(raw)
            plain=b''.join(v[2] for v in base.values())+cold.record('TRAILER!!!',b'',0,9999)
            newparts[1]=gzip.compress(plain,mtime=0);struct.pack_into('<I',head,16,len(newparts[1]))
            h=hashlib.sha1()
            for part in newparts:h.update(part);h.update(struct.pack('<I',len(part)))
            head[576:608]=h.digest()+bytes(12)
            image=(bytes(head)+b''.join(part+bytes(cold.align(len(part),2048)-len(part)) for part in newparts)).ljust(len(raw),b'\0')
            path=work/'original.img';path.write_bytes(image)
            services={k:a.inputs/(v+'.rc') for k,v in [('sf','surfaceflinger'),('hwc','hwcomposer'),('allocator','allocator'),('zygote','zygote')]}
            cold.prepare(path,a.package/'bin/cold_boot64',a.package,out=work/'prepared',**services)
            prepared=json.loads((work/'prepared/PREPARED.json').read_text())
            assert prepared['root_backend']=='magisk-overlay-postfs' and not prepared['root_hardware_acceptance']
            candidate=(work/'prepared/candidate.img').read_bytes();cold.validate_firmware(candidate,True)
            after,_=cold.cpio_records(cold.unpack_ramdisk(cold.unpack_boot(candidate)[1][1])[0])
            assert all(after[k][2]==v[2] for k,v in base.items())
        results.append({'case':profile['id']+' complete preparation','passed':True})
        if profile['version_code']<28000:
            without32={k:v for k,v in base.items() if k!='overlay.d/sbin/magisk32.xz'}
            assert cold.validate_root(without32)['layout']=='split'
            results.append({'case':profile['id']+' without optional ARM32 root binary','passed':True})
        # Local rebuilds/nightlies are inputs, not upstream hash matches.
        local=dict(base);fields,data,_=local['init'];local['init']=(fields,data+b'local nightly build',b'')
        assert cold.validate_root(local)['payload_digests']['init']!=cold.validate_root(base)['payload_digests']['init']
        results.append({'case':profile['id']+' accepts locally rebuilt init','passed':True})
        broken=dict(base);fields,data,_=broken['init'];broken['init']=(fields,b'BAD!'+data[4:],b'')
        try:cold.validate_root(broken)
        except ValueError:pass
        else:raise AssertionError('Invalid ELF accepted')
        results.append({'case':profile['id']+' rejects invalid ELF','passed':True})
        configured=dict(base);f,data,_=configured['.backup/.magisk']
        configured['.backup/.magisk']=(f,data+b'FUTURE_NIGHTLY_OPTION=enabled\n',b'')
        assert cold.validate_root(configured)['backend']=='magisk-overlay-postfs'
        results.append({'case':profile['id']+' preserves additional root setting','passed':True})
        table=dict(base);f,data,_=table['init'];data=bytearray(data)
        struct.pack_into('<I' if data[4]==1 else '<Q',data,28 if data[4]==1 else 32,len(data)+1)
        table['init']=(f,bytes(data),b'')
        try:cold.validate_root(table)
        except ValueError:pass
        else:raise AssertionError('Out-of-bounds ELF program table accepted')
        results.append({'case':profile['id']+' rejects truncated program table','passed':True})
        raw_payloads=dict(base)
        for name in list(raw_payloads):
            if name=='.backup/init.xz' or name.startswith('overlay.d/sbin/') and name.endswith('.xz') and 'stub' not in name:
                fields,data,_=raw_payloads.pop(name);fields=list(fields)
                fields[1]=0o100600 if name.startswith('.backup/') else 0o100700
                raw_payloads[name[:-3]]=(fields,lzma.decompress(data),b'')
        assert cold.validate_root(raw_payloads)['backend']=='magisk-overlay-postfs'
        results.append({'case':profile['id']+' accepts raw root executables and private init backup','passed':True})
    report={'status':'OFFLINE_ROOT_IMPLEMENTATION_CASES_ONLY','profiles':len(samples),'cases':results,
            'real_boot_written':False,'other_root_versions_booted':False,'kernelsu_apatch_first_binding_supported':False}
    (a.out/'results-private.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',len(results),'root implementation cases; no real boot/root changes')


if __name__=='__main__':main()
