"""Package both exact-image E-DDC repair and native initialization components.

Private generated image contracts stay in the kernel build directories.
This does not install or assert HDMI, HDR or QMS acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('repair','recovery','work'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
    values={}
    for src,name,kind in [(a.repair,'rk_decode_ddc.ko','native_HDMI_E_DDC_segment_repair'),
                          (a.recovery,'rk_decode_ddc_recover.ko','native_HDMI_reinitialization_after_EDID_repair')]:
        meta=json.loads((src/'BUILD.json').read_text(encoding='utf-8'));ko=src/name
        if meta.get('kind')!=kind or sha(ko)!=meta.get('ko_sha256'):
            raise RuntimeError('Complete checked native component required: '+name)
        shutil.copyfile(ko,w/name);values[name]=sha(ko)
    (w/'CANDIDATE.json').write_text(json.dumps({'kind':'exact_image_Decode_E_DDC_and_native_initialization',
        'artifacts':values,'physical_HDMI_accepted':False,'HDR_accepted':False,'QMS_accepted':False,
        'firmware_kernel_image_sha256':'d9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740',
        'rollback':'display_ddc=0 and reboot, or disable the full Decode Stack and reboot',
        'upstream_reference':'https://lkml.iu.edu/2406.0/06496.html',
        'upstream_vendor_commit':'cdc0984c90dc'},indent=2)+'\n',encoding='utf-8',newline='\n')
    (w/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.name+'\n' for f in sorted(w.iterdir()) if f.is_file()),
        encoding='ascii',newline='\n')
    print(json.dumps({'component':str(w),'artifacts':values}))

if __name__=='__main__':main()
