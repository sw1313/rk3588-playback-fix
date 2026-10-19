"""Build the independent Surface/TextureView diagnostic with a local test key."""
from pathlib import Path
import argparse
import os
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True, help='New output directory')
    p.add_argument('--keystore', type=Path, required=True, help='Local diagnostic key, never commit')
    p.add_argument('--source', type=Path, help='Optional isolated diagnostic source directory with its own manifest')
    a = p.parse_args()
    source = a.source.resolve() if a.source else Path(__file__).resolve().parent
    sdk, work = a.sdk.resolve(), a.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    platform = sorted(sdk.glob('platforms/android-*/android.jar'),
                      key=lambda x: tuple(int(n) for n in x.parent.name.split('-')[1].split('.')))[-1]
    bt = sorted((sdk / 'build-tools').iterdir(),
                key=lambda x: tuple(int(n) for n in x.name.split('.')))[-1]
    ext = '.exe' if os.name == 'nt' else ''

    def run(*args):
        subprocess.run([str(x) for x in args], check=True)

    classes = work / 'classes'
    classes.mkdir()
    run('javac', '-source', '8', '-target', '8', '-encoding', 'UTF-8',
        '-classpath', platform, '-d', classes, source / 'MainActivity.java')
    jar = work / 'classes.jar'
    run('jar', 'cf', jar, '-C', classes, '.')
    dex = work / 'dex'
    dex.mkdir()
    # Use Java entry points to avoid Windows batch-file shell quoting.
    run('java', '-cp', bt / 'lib/d8.jar', 'com.android.tools.r8.D8',
        '--lib', platform, '--min-api', '32', '--output', dex, jar)
    unsigned = work / 'unsigned.apk'
    run(bt / ('aapt2' + ext), 'link', '-I', platform,
        '--manifest', source / 'AndroidManifest.xml', '-o', unsigned)
    import zipfile
    with zipfile.ZipFile(unsigned, 'a', zipfile.ZIP_DEFLATED) as apk:
        apk.write(dex / 'classes.dex', 'classes.dex')
    aligned = work / 'aligned.apk'
    run(bt / ('zipalign' + ext), '-f', '4', unsigned, aligned)
    if not a.keystore.exists():
        a.keystore.parent.mkdir(parents=True, exist_ok=True)
        run('keytool', '-genkeypair', '-keystore', a.keystore, '-storepass', 'android',
            '-keypass', 'android', '-alias', 'diagnostic', '-keyalg', 'RSA',
            '-validity', '3650', '-dname', 'CN=Local Playback Diagnostic')
    output = work / 'rk-display-path.apk'
    run('java', '-jar', bt / 'lib/apksigner.jar', 'sign', '--ks', a.keystore,
        '--ks-pass', 'pass:android', '--key-pass', 'pass:android', '--out', output, aligned)
    run('java', '-jar', bt / 'lib/apksigner.jar', 'verify', output)
    print(output)


if __name__ == '__main__':
    main()
