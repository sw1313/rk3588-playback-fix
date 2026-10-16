"""Verify actual release installers carry matching licenses and source provenance."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import zipfile
from package_licenses import ROOT, DOCUMENTS, metadata


def verify():
    manifest = json.loads((ROOT / 'release-manifest.json').read_text(encoding='utf-8'))
    assert len(manifest['components']) == 4
    results = []
    for item in manifest['components']:
        path = ROOT / 'dist' / item['filename']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], path.name
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            assert len(names) == len(set(names)), path.name
            assert archive.testzip() is None, path.name
            assert all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts for n in names)
            prefix = 'assets/open-source/' if item['id'] == 'yamby-mpv-plugin' else ''
            for name in DOCUMENTS:
                assert archive.read(prefix + name) == (ROOT / name).read_bytes().replace(b'\r\n', b'\n'), (path.name, name)
            provenance = json.loads(archive.read(prefix + 'OPEN_SOURCE.json'))
            assert provenance == metadata(item['id'])
            assert provenance['component_version'] == item['version']
            assert provenance['project_release'] == manifest['release_tag']
            checksum_count = 0
            for name in names:
                if PurePosixPath(name).name != 'checksums.sha256':
                    continue
                base = PurePosixPath(name).parent
                for line in archive.read(name).decode('utf-8').splitlines():
                    expected, relative = line.split(None, 1)
                    actual = hashlib.sha256(archive.read((base / relative).as_posix())).hexdigest()
                    assert actual == expected, (path.name, name, relative)
                    checksum_count += 1
            license_count = 1
            if item['id'] == 'decode-stack':
                for source in (ROOT / 'LICENSES').rglob('*'):
                    if source.is_file():
                        assert archive.read(source.relative_to(ROOT).as_posix()) == source.read_bytes().replace(b'\r\n', b'\n')
                        license_count += 1
                checked = {line.split(None, 1)[1] for line in archive.read('checksums.sha256').decode().splitlines()}
                assert set(DOCUMENTS) | {'OPEN_SOURCE.json'} <= checked
            if item['id'] == 'mali-global':
                assert not any(n.endswith('/libGLES_mali.so') for n in names)
                assert archive.read('LICENSES/Apache-2.0.txt') == (ROOT / 'third_party/Apache-2.0.txt').read_bytes().replace(b'\r\n', b'\n')
            if prefix:
                assert not any(n.startswith('lib/') for n in names), 'Unexpected player/framework native library'
            results.append({'component': item['id'], 'version': item['version'],
                            'documents': len(DOCUMENTS) + 1, 'license_texts_checked': license_count,
                            'checksum_entries_checked': checksum_count, 'passed': True})
    return results


if __name__ == '__main__':
    print(json.dumps(verify(), indent=2))
