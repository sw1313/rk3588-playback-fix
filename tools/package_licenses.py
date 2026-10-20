"""Stage public notices before package integrity manifests are generated."""
from pathlib import Path
import argparse
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
DOCUMENTS = ('LICENSE', 'NOTICE', 'OPEN_SOURCE.md', 'THIRD_PARTY_NOTICES.md',
             'SOURCE_CODE.md', 'SOURCE_CODE.zh-CN.md')
COMPONENTS = {
    'panvk-app': ('panvk', '0.1.0-rc3-rk3588-a12-test8.1'),
    'decode-stack': ('magisk-decode', None),
    'mali-global': ('magisk-mali-global', None),
    'frameguard': ('magisk', None),
    'yamby-mpv-plugin': ('lsposed', '0.3.7'),
}
RELEASE_TAG = 'v0.8.8'
URL = 'https://github.com/sw1313/rk3588-playback-fix'


def metadata(component, source_ref=None):
    folder, version = COMPONENTS[component]
    if version is None:
        fields = dict(line.split('=', 1) for line in
                      (ROOT / folder / 'module.prop').read_text(encoding='utf-8').splitlines()
                      if '=' in line)
        version = fields['version']
    result = {
        'component': component, 'component_version': version,
        'project_release': RELEASE_TAG, 'original_project_license': 'MIT',
        'source': URL + '/tree/' + RELEASE_TAG,
        'build_guide': URL + '/blob/' + RELEASE_TAG + '/SOURCE_CODE.md',
        'third_party_licenses_retained': True,
        'proprietary_driver_or_firmware_included': False,
    }
    if source_ref == 'working-tree':
        result.update(project_release='unreleased', source='local working tree',
                      build_guide='SOURCE_CODE.md', source_note='Candidate source archive accompanies this local build; no published release is claimed.')
    elif source_ref:
        result.update(project_release=source_ref, source=URL + '/tree/' + source_ref,
                      build_guide=URL + '/blob/' + source_ref + '/SOURCE_CODE.md')
    return result


def stage(destination, component, source_ref=None):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for name in DOCUMENTS:
        (destination / name).write_bytes((ROOT / name).read_bytes().replace(b'\r\n', b'\n'))
    if component == 'decode-stack':
        shutil.copytree(ROOT / 'LICENSES', destination / 'LICENSES', dirs_exist_ok=True)
    elif component == 'mali-global':
        (destination / 'LICENSES').mkdir(exist_ok=True)
        shutil.copyfile(ROOT / 'third_party/Apache-2.0.txt', destination / 'LICENSES/Apache-2.0.txt')
        for name in ['GPL-2.0-only.txt','Linux-syscall-note.txt']:
            shutil.copyfile(ROOT / 'LICENSES' / name, destination / 'LICENSES' / name)
    (destination / 'OPEN_SOURCE.json').write_text(
        json.dumps(metadata(component, source_ref), indent=2) + '\n', encoding='utf-8', newline='\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('component', choices=COMPONENTS)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    stage(args.destination, args.component)
