"""Private exact-Image contract for the native HDMI E-DDC algorithm wrapper."""
import struct

PIN = 'd9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740'
BASE = 0xffffffc010000000

def generate(image, elf, output):
    import hashlib
    from elftools.elf.elffile import ELFFile
    from build_display_probe import fnv
    raw=image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=PIN:raise RuntimeError('Image mismatch')
    with elf.open('rb') as f:symbols=list(ELFFile(f).get_section_by_name('.symtab').iter_symbols())
    byname={}
    for s in symbols:byname.setdefault(s.name,set()).add(s['st_value'])
    anchor=next(iter(byname['printk']))
    # The QP implementation is distinguished from legacy HDMI by the exact
    # read register/control constants and its known full image fingerprint.
    candidates=sorted(byname['dw_hdmi_i2c_xfer'])
    if len(candidates)!=2:raise RuntimeError('Unexpected xfer symbol set')
    xfer=candidates[1]
    algorithms=[]
    for a in byname['dw_hdmi_algorithm']:
        if struct.unpack_from('<Q',raw,a-BASE)[0]==xfer:algorithms.append(a)
    if len(algorithms)!=1:raise RuntimeError('Ambiguous QP algorithm')
    algo=algorithms[0]
    names=['printk','i2c_get_adapter','i2c_put_adapter','i2c_transfer',
           'dw_hdmi_qp_main_hardirq','wait_for_completion_timeout']
    addresses=[xfer]
    for name in names:
        if len(byname[name])!=1:raise RuntimeError('Ambiguous '+name)
        addresses+=list(byname[name])
    lines=['struct ddc_function { long delta; unsigned int size; unsigned long long hash; };',
           'static const struct ddc_function ddc_functions[] = {']
    report=[]
    all_addresses=sorted(set(s['st_value'] for s in symbols))
    for a in addresses:
        end=next(x for x in all_addresses if x>a)
        # Full native xfer/IRQ; exported API prefixes qualify ABI anchors.
        size=end-a if a in [xfer,next(iter(byname['dw_hdmi_qp_main_hardirq']))] else min(64,end-a)
        h=fnv(raw[a-BASE:a-BASE+size])
        lines.append('    {%dL, %dU, 0x%xULL},'%(a-anchor,size,h))
        report.append({'symbol':next(s.name for s in symbols if s['st_value']==a),'size':size})
    lines+=['};','#define DDC_ALGO_DELTA (%dL)'%(algo-anchor),
            'static const long ddc_algo_slots[5] = {']
    lines+=['    %dL,'%((p-anchor) if p else 0) for p in struct.unpack_from('<5Q',raw,algo-BASE)]
    lines+=['};']
    output.write_text('\n'.join(lines)+'\n',encoding='ascii',newline='\n')
    return report
