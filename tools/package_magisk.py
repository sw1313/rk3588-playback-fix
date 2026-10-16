"""Package Frame Guard with public notices and ZIP-root entries."""
from pathlib import Path
import tempfile
import zipfile
from package_licenses import stage

root = Path(__file__).resolve().parents[1]
destination = root / 'dist'
destination.mkdir(exist_ok=True)
fields = dict(line.split('=', 1) for line in
              (root / 'magisk/module.prop').read_text(encoding='utf-8').splitlines()
              if '=' in line)
archive = destination / ('rk3588-frameguard-' + fields['version'] + '.zip')
names = ('module.prop', 'customize.sh', 'control.sh', 'service.sh',
         'action.sh', 'uninstall.sh', 'skip_mount')
with tempfile.TemporaryDirectory() as temporary:
    package = Path(temporary)
    for name in names:
        (package / name).write_bytes((root / 'magisk' / name).read_bytes().replace(b'\r\n', b'\n'))
    stage(package, 'frameguard')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for path in sorted(package.rglob('*')):
            if path.is_file():
                output.write(path, path.relative_to(package).as_posix())
print(archive)
