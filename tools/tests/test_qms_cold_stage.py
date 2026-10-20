"""New candidate-only pre-exec rendezvous cases in a confined Android fixture.

RK_COLD_TEST cannot bind a real GPU, resume a real process, set a property,
write a block device or reboot. Display loader is a hashed fixture script,
not a QMS module. This does not qualify the real boot/rollback integration.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = '/data/local/tmp/rk-cold-fixture'
REMOTE = '/data/local/tmp/rk-qms-cold-fixture64'
TAR = '/data/local/tmp/rk-qms-cold-fixture.tar'
BOOT = 'fixture-boot-uuid'
PROFILE = 'b' * 64
IMAGE = 'd9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740'


def sha(data):
    return hashlib.sha256(data if isinstance(data, bytes) else data.encode()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('adb', 'serial'):
        p.add_argument('--' + key, required=True)
    p.add_argument('--su', default='su', help='Verified on-device su executable; supports original-root recovery PATH')
    p.add_argument('--adb-port', default='5037')
    for key in ('ndk', 'out'):
        p.add_argument('--' + key, type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    binary = a.out / 'qms-cold-fixture64'
    compiler = a.ndk / 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
    subprocess.run([str(compiler), '--target=aarch64-linux-android32', '-DRK_COLD_TEST',
                    '-DRKQMS_DISPLAY_STAGE=1', '-O2', '-static', '-Wall', '-Wextra', '-Werror',
                    str(ROOT / 'magisk-mali-global/src/cold_boot.c'), '-o', str(binary)], check=True)
    adb = [a.adb, '-P', a.adb_port, '-s', a.serial]

    def shell(cmd, check=True):
        return subprocess.run(adb + ['shell', shlex.quote(a.su)+' -c ' + shlex.quote(cmd)], capture_output=True,
                              text=True, encoding='utf8', errors='replace', timeout=30, check=check)

    def push(src, dst):
        subprocess.run(adb + ['push', str(src), dst], capture_output=True, check=True, timeout=30)

    if shell('id -u').stdout.strip()!='0':raise RuntimeError('Root fixture access unavailable')
    for path in (BASE, REMOTE, TAR):
        if shell('test ! -e ' + path, False).returncode:
            raise RuntimeError('Refusing to overwrite existing fixture: ' + path)
    original = b'O' * 4096
    candidate = b'C' * 4096
    kmd, csf, g32, g64 = b'fake-kmd', b'fake-csf', b'fake32', b'fake64'
    profile = '\n'.join(k + ' ' + v for k, v in {
        'profile_id': 'a' * 64, 'original_sha256': sha(original), 'original_bytes': '4096',
        'kmd_sha256': sha(kmd), 'csf_sha256': sha(csf), 'gles32_sha256': sha(g32),
        'gles64_sha256': sha(g64)}.items()) + '\n'
    helper = ('#!/system/bin/sh\n' +
              'printf "%s %s prepared\\n" "$2" "$3" > ' + BASE + '/display-state/receipt\n' +
              'cat ' + BASE + '/display-state/receipt\n')
    defaults = {'state/boot/original.img': original, 'state/boot/candidate.img': candidate,
        'state/boot/image.sha256': sha(candidate) + '\n', 'state/boot/profile': profile,
        'boot-device': candidate, 'boot-id': BOOT + '\n', 'state/mode': 'on\n',
        'state/kernel-mode': 'next\n', 'state/cold-mode': 'on\n', 'state/cold-plan': BOOT + '\n',
        'module/active-mode': 'on\n', 'module/kernel/rk_mali_next.ko': kmd,
        'module/payload/lib/egl/libGLES_mali.so': g32,
        'module/payload/lib64/egl/libGLES_mali.so': g64,
        'state/cache/' + sha(csf) + '.bin': csf, 'firmware': csf, 'gles32': g32, 'gles64': g64,
        'decode/config': 'mpp=1\ncodec2=1\ndisplay_qms=1\n', 'decode/active-boot-id': BOOT + '\n',
        'decode/components/qms/boot-stage64': helper,
        'decode/components/qms/rk_decode_qms.ko': b'NOT A KERNEL MODULE',
        'display-hwc': b'NOT A HARDWARE COMPOSER', 'display-state/plan': BOOT + ' ' + PROFILE + ' prepared\n'}
    for pid, service in ((101, 'surfaceflinger'), (102, 'vendor.hwcomposer-2-1'), (103, 'vendor.gralloc-4-0')):
        defaults['properties/init.svc_debug_pid.' + service] = str(pid) + '\n'
        defaults['process-' + str(pid)] = 'State:\tT (stopped)\nPPid:\t1\n'
        defaults['process-exe-' + str(pid)] = 'init\n'
    results = []

    def case(name, changes, expected, event, disabled=False, loader_started=False, modes=None, pending_after=None):
        files = defaults | changes
        payload_profile = {'profile_id': PROFILE,
            'helper_sha256': sha(files.get('decode/components/qms/boot-stage64') or b''),
            'module_sha256': sha(files.get('decode/components/qms/rk_decode_qms.ko') or b''),
            'hwc_sha256': sha(files.get('display-hwc') or b''), 'image_sha256': IMAGE}
        if 'decode/components/qms/profile' not in changes:
            files['decode/components/qms/profile'] = ''.join(k + ' ' + v + '\n' for k, v in payload_profile.items())
        with tempfile.TemporaryDirectory() as td:
            archive = Path(td) / 'fixture.tar'
            folder = Path(td) / 'fixture'
            folder.mkdir()
            for name_, data in files.items():
                if data is None:
                    continue
                f = folder / name_
                f.parent.mkdir(parents=True, exist_ok=True)
                f.write_bytes(data.encode() if isinstance(data, str) else data)
            with tarfile.open(archive, 'w') as tar:
                for f in folder.rglob('*'):
                    if not f.is_file():
                        continue
                    info = tar.gettarinfo(str(f), f.relative_to(folder).as_posix())
                    info.uid = info.gid = 0
                    info.mode = 0o700 if f.name == 'boot-stage64' else 0o600
                    info.mode = (modes or {}).get(f.relative_to(folder).as_posix(), info.mode)
                    with f.open('rb') as stream:
                        tar.addfile(info, stream)
            push(archive, TAR)
        shell('mkdir ' + BASE + ' && tar -xf ' + TAR + ' -C ' + BASE)
        try:
            result = shell(REMOTE + ' stage', False)
            if result.returncode != expected:
                raise RuntimeError((name, result.returncode, result.stderr))
            journal = shell('cat ' + BASE + '/state/cold-journal').stdout
            if event not in journal:
                raise RuntimeError((name, 'missing event', journal))
            if ('DECODE_QMS_PREEXEC_PREPARE_STARTED' in journal) != loader_started:
                raise RuntimeError((name, 'unexpected loader execution'))
            if expected:
                if 'CONTROL ctl.sigstop_off' in journal or 'CONTINUE ' in journal or 'LOADING_KMD' in journal:
                    raise RuntimeError((name, 'released graphics or loaded GPU after failure'))
            if 'DECODE_QMS_PREPARED_BEFORE_GRAPHICS_RELEASE' in journal:
                if not (journal.index('DECODE_QMS_PREPARED_BEFORE_GRAPHICS_RELEASE') <
                        journal.index('LOADING_KMD') < journal.index('CONTROL ctl.sigstop_off')):
                    raise RuntimeError((name, 'wrong preparation/release ordering'))
            if disabled:
                config = shell('cat ' + BASE + '/decode/config').stdout
                if config != 'mpp=1\ncodec2=1\ndisplay_qms=0\n':
                    raise RuntimeError((name, 'failed to preserve independent selections', config))
            if shell('sha256sum ' + BASE + '/boot-device').stdout.split()[0] != sha(candidate):
                raise RuntimeError((name, 'display failure touched GPU boot image'))
            if pending_after is not None:
                pending=shell('cat '+BASE+'/state/boot-pending',False)
                if pending_after=='absent':
                    if pending.returncode==0:raise RuntimeError((name,'unstarted GPU pending retained'))
                elif pending.stdout.strip()!=pending_after:raise RuntimeError((name,'foreign pending changed'))
            results.append({'name': name, 'exit': expected, 'passed': True})
        finally:
            shell('rm -r ' + BASE)

    push(binary, REMOTE)
    shell('chmod 0700 ' + REMOTE)
    try:
        case('missing optional Decode config preserves existing GPU sequence', {'decode/config': None}, 0,
             'COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART')
        case('absent selection preserves native display', {'decode/config': 'mpp=1\ncodec2=1\n'}, 0,
             'COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART')
        case('explicit off preserves native display', {'decode/config': 'mpp=1\ncodec2=1\ndisplay_qms=0\n'}, 0,
             'COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART')
        case('disabled Decode bypasses display loader', {'decode/disable': ''}, 0,
             'COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART')
        case('removed Decode bypasses display loader', {'decode/remove': ''}, 0,
             'COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART')
        case('display acknowledgement precedes GPU and graphics', {}, 0,
             'DECODE_QMS_PREPARED_BEFORE_GRAPHICS_RELEASE', loader_started=True)
        case('unqualified previous display trial reboots to native overlays', {'display-state/pending': 'old-boot\n'}, 30,
             'DECODE_QMS_PREVIOUS_UNQUALIFIED_DISPLAY_ROLLBACK', True)
        case('qualified previous trial may prepare again', {'display-state/pending': 'old-boot\n', 'display-state/qualified': 'old-boot\n'}, 0,
             'DECODE_QMS_PREPARED_BEFORE_GRAPHICS_RELEASE', loader_started=True)
        case('stale overlay plan disables only display', {'display-state/plan': 'old-boot ' + PROFILE + ' prepared\n'}, 30,
             'DECODE_QMS_CURRENT_OVERLAY_PLAN_MISSING', True)
        case('stale active overlay boot refuses', {'decode/active-boot-id': 'old-boot\n'}, 30,
             'DECODE_QMS_CURRENT_OVERLAY_PLAN_MISSING', True)
        case('wrong kernel profile refuses', {'decode/components/qms/profile': 'image_sha256 ' + 'a' * 64 + '\n'}, 30,
             'DECODE_QMS_PAYLOAD_OR_HWC_MISMATCH', True)
        case('missing kernel payload refuses', {'decode/components/qms/rk_decode_qms.ko': None}, 30,
             'DECODE_QMS_PAYLOAD_OR_HWC_MISMATCH', True)
        case('missing display loader refuses', {'decode/components/qms/boot-stage64': None}, 30,
             'DECODE_QMS_PAYLOAD_OR_HWC_MISMATCH', True)
        case('duplicate selection fails closed', {'decode/config': 'mpp=1\ncodec2=1\ndisplay_qms=1\ndisplay_qms=0\n'}, 30,
             'DECODE_QMS_INVALID_SELECTION', True)
        case('loader error never releases graphics', {'decode/components/qms/boot-stage64': '#!/system/bin/sh\nexit 1\n'}, 30,
             'DECODE_QMS_BIND_FAILED', True, True)
        case('wrong receipt never releases graphics', {'decode/components/qms/boot-stage64': '#!/system/bin/sh\necho wrong\n'}, 30,
             'DECODE_QMS_BIND_RECEIPT_MISMATCH', True, True)
        case('changed pre-exec process refuses release', {'decode/components/qms/boot-stage64': helper +
             'printf "State: R\\nPPid: 1\\n" > ' + BASE + '/process-101\n'}, 30,
             'DECODE_QMS_GATE_CHANGED_DURING_BIND', True, True)
        case('writable config rejected without disabling safeguards', {}, 76,
             'DECODE_QMS_DISABLE_FAILED_GRAPHICS_REMAIN_HELD', modes={'decode/config': 0o666})
        case('loader timeout kills its process group and requests display rollback',
             {'decode/components/qms/boot-stage64': '#!/system/bin/sh\nsleep 30\n'}, 30,
             'DECODE_QMS_BIND_FAILED', True, True)
        case('display-only reboot clears only current unstarted GPU pending',
             {'state/boot-pending': BOOT+'\n','display-state/plan':'stale\n'},30,
             'DECODE_QMS_UNSTARTED_GPU_TRIAL_DEFERRED',True,pending_after='absent')
        case('display-only reboot never clears another boot pending',
             {'state/boot-pending':'older-boot\n','display-state/plan':'stale\n'},30,
             'DECODE_QMS_GLOBAL_PENDING_NOT_CURRENT',True,pending_after='older-boot')
        report = {'status': 'ISOLATED_CANDIDATE_RENDEZVOUS_ONLY', 'cases': results,
                  'hardware_loader_supplied': False, 'real_boot_or_display_modified': False,
                  'QMS_hardware_acceptance': False}
        (a.out / 'RESULT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('PASS', len(results), 'isolated QMS rendezvous cases; no real boot/display changes')
    finally:
        shell('rm -f ' + REMOTE + ' ' + TAR)


if __name__ == '__main__':
    main()
