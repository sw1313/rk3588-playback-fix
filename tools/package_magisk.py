"""Package the Magisk module with ZIP-root entries and LF line endings."""
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
destination = root / 'dist'
destination.mkdir(exist_ok=True)
archive = destination / 'rk3588-frameguard-0.1.0.zip'
names = ('module.prop', 'customize.sh', 'control.sh', 'service.sh',
         'action.sh', 'uninstall.sh', 'skip_mount')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for name in names:
        data = (root / 'magisk' / name).read_bytes().replace(b'\r\n', b'\n')
        output.writestr(name, data)
print(archive)
