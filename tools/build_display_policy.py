"""Build exact API32 system display policy; firmware input is never packaged."""
import argparse, hashlib, json, shutil, subprocess, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PIN = '922c443c3a2b609b7f7f3755bbfbeb90f3c0b0c141829f3b6151371c2aa607b6'
DEPS = {
    'dexlib2-2.5.2.jar': '5a5c8982d8bd7d6e3bb1a0713049e3c78b719ec32b20f6b619885cec30a0dd61',
    'guava-27.1-android.jar': '686404f2d1d4d221911f96bd627ff60dac2226a5dfa6fb8ba517073eb97ec0ef',
    'failureaccess-1.0.1.jar': 'a171ee4c734dd2da837e4b16be9df4661afab72a41adaf31eb84dfdaf936ca26',
}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args): subprocess.run(list(map(str, args)), check=True)
def jar(classes, target):
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(classes.rglob('*.class')): z.write(f, f.relative_to(classes).as_posix())
def dex(d8, output, *jars):
    output.mkdir(); run('java', '-cp', d8, 'com.android.tools.r8.D8', '--min-api', '32', '--output', output, *jars)
def dexjar(folder, target):
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(folder.glob('*.dex')): z.write(f, f.name)
def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['services','android-jar','d8','deps','licenses','work']: p.add_argument('--'+name, type=Path, required=True)
    a=p.parse_args(); w=a.work.resolve()
    if w.exists(): raise SystemExit('Use fresh work directory')
    if sha(a.services)!=PIN: raise SystemExit('Unknown services.jar')
    for name, digest in DEPS.items():
        if sha(a.deps/name)!=digest: raise SystemExit('Dependency mismatch: '+name)
    out=w/'component'; shutil.copytree(ROOT/'magisk-display-policy',out)
    (out/'contracts').mkdir(); (out/'licenses').mkdir()
    # dexlib2 BSD and Guava/failureaccess Apache-2.0. Annotation-only jsr305 is not bundled.
    for name in ['dexlib2.txt','guava.txt']: shutil.copyfile(a.licenses/name,out/'licenses'/name)
    for name in ['stub','helper','patcher','probe']: (w/name).mkdir()
    stub=ROOT/'tools/display-policy/stubs/com/android/server/display/DisplayModeDirector.java'
    run('javac','--release','8','-d',w/'stub',stub)
    run('javac','--release','8','-cp',str(a.android_jar)+';'+str(w/'stub'),'-d',w/'helper',out/'src/RkDisplayModeLocale.java')
    jar(w/'helper',w/'helper.jar'); dex(a.d8,w/'helper-dex',w/'helper.jar')
    shutil.copyfile(w/'helper-dex/classes.dex',out/'helper.dex')
    cp=';'.join(str(a.deps/name) for name in DEPS)
    patch=ROOT/'tools/display-policy/PatchServices.java'; probe=ROOT/'tools/display-policy/AbiProbe.java'
    run('javac','--release','8','-cp',cp,'-d',w/'patcher',patch)
    run('javac','--release','8','-d',w/'probe',probe)
    jar(w/'patcher',w/'patcher-host.jar');jar(w/'probe',w/'probe-host.jar')
    dex(a.d8,w/'patcher-dex',w/'patcher-host.jar',*[a.deps/n for n in DEPS]);dexjar(w/'patcher-dex',out/'patcher.jar')
    dex(a.d8,w/'probe-dex',w/'probe-host.jar');dexjar(w/'probe-dex',out/'probe.jar')
    variant=w/'services-patched-private.jar'
    run('java','-cp',str(w/'patcher-host.jar')+';'+cp,'com.scrcpyandroid.displaypolicy.PatchServices',a.services,variant,out/'helper.dex')
    with zipfile.ZipFile(variant) as z:
        entries=sorted(n for n in z.namelist() if n.endswith('.dex'))
        (out/'contracts/dex.sha256').write_text(''.join(hashlib.sha256(z.read(n)).hexdigest()+'  '+n+'\n' for n in entries),encoding='ascii')
    for src in [patch,probe,stub]:shutil.copyfile(src,out/'src'/src.name)
    meta={'status':'UNVALIDATED_EXACT_FIRMWARE_CANDIDATE','firmware_services_sha256':PIN,'dependencies':DEPS,'scope':'Four locale formatters, native setter delegation, fixed-mode policy, connector-specific Auto selection read and truthful persistent HDR choice; Auto delegates native mode policy','private_firmware_bundled':False}
    (out/'CANDIDATE.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
    for f in out.rglob('*'):
        if f.is_file() and f.suffix in {'.sh','.java','.txt','.sha256','.json'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
    (out/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in sorted(out.rglob('*')) if f.is_file()),encoding='ascii',newline='\n')
    print('BUILT source-only display-policy component; private firmware output remains outside component')
if __name__=='__main__':main()
