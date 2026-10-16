"""Exercise real Android sh in an isolated fixture; never mount or edit the live module.

Usage: python tools/tests/test_decode_webui.py --adb /path/to/adb
Only fixed module-directory literals are redirected into the disposable fixture.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / 'magisk-decode'
    fixture = '/data/local/tmp/rk-webui-test064'
    current = fixture + '/modules/rk3588_decode_stack'
    staged = fixture + '/modules_update/rk3588_decode_stack'
    keys = 'mpp rga codec2 libyuv software hdr_static hdr10plus codec2_sdk codec2_buffer p010_fast codec2_sdk_next av1_native av1_grain av1_afbc'.split()
    assertions = 0

    def adb(*parts, check=True):
        return subprocess.run([args.adb, *parts], check=check, capture_output=True, text=True, encoding='utf-8')

    def shell(command, check=True):
        return adb('shell', command, check=check)

    def put(path, data):
        local = Path(temp.name) / 'input'
        local.write_text(data, encoding='utf-8', newline='\n')
        adb('push', str(local), path)

    def expect(condition, label):
        nonlocal assertions
        assert condition, label
        assertions += 1

    def call(*parts, check=True):
        return shell('sh ' + current + '/webui.sh ' + ' '.join(shlex.quote(x) for x in parts), check=check)

    def status():
        return json.loads(call('status').stdout)

    def set_flag(key, enabled):
        return json.loads(call('set', key, 'on' if enabled else 'off').stdout)

    def config(**changes):
        return ''.join(k + '=' + str(changes.get(k, 1)) + '\n' for k in keys)

    expect(shell('test ! -e ' + fixture, check=False).returncode == 0, 'fresh fixture required')
    with tempfile.TemporaryDirectory() as directory:
        temp = type('Temp', (), {'name': directory})()
        try:
            shell('mkdir -p ' + current + '/components/codec2_sdk/system/vendor/lib/hw ' + current + '/components/codec2_sdk_next/system/vendor/lib/hw')
            for name in ['common.sh', 'mode-control.sh', 'action.sh', 'webui.sh', 'module.prop']:
                data = (source / name).read_text(encoding='utf-8')
                data = data.replace('/data/adb/modules_update', fixture + '/modules_update').replace('/data/adb/modules', fixture + '/modules')
                put(current + '/' + name, data)
            for component in ['codec2_sdk', 'codec2_sdk_next']:
                put(current + '/components/' + component + '/system/vendor/lib/hw/libcodec2_rx_component.so', 'fixture-not-a-library')
            put(current + '/config', config())
            unknown = status()
            expect(not unknown['activeKnown'] and unknown['pendingReboot'], 'legacy/first install never claims active')
            put(current + '/active-config', config())
            put(current + '/active-version', '0.6.4\n')
            shell('cat /proc/sys/kernel/random/boot_id > ' + current + '/active-boot-id')
            put(current + '/status.txt', 'PASS unified decode stack, fixture only\n')
            ready = status()
            expect(ready['activeKnown'] and ready['activeEnabled'] and not ready['pendingReboot'], 'matching boot receipt')
            expect(ready['health'] == 'passed', 'bounded health status')
            original_active = shell('cat ' + current + '/active-config').stdout
            for key in keys:
                changed = set_flag(key, False)
                expect(changed['components'][key]['requested'] == 0, key + ' saved off')
                expect(changed['pendingReboot'] and changed['components'][key]['active'] == 1, key + ' does not activate live')
                restored = set_flag(key, True)
                expect(not restored['pendingReboot'], key + ' restored without unnecessary reboot')
            off = set_flag('mpp', False)
            for key in ['codec2', 'codec2_sdk', 'codec2_sdk_next', 'codec2_buffer', 'software', 'hdr_static', 'hdr10plus', 'p010_fast', 'av1_native', 'av1_grain', 'av1_afbc']:
                expect(off['components'][key]['requested'] == 1 and off['components'][key]['effective'] == 0, key + ' dependency suppression preserves selection')
            set_flag('mpp', True)
            off = set_flag('rga', False)
            expect(off['components']['codec2']['effective'] == 0 and off['components']['av1_grain']['effective'] == 1, 'RGA dependency does not suppress independent grain')
            set_flag('rga', True)
            off = set_flag('codec2_sdk_next', False)
            expect(off['components']['codec2_sdk']['effective'] == 1 and off['components']['av1_native']['effective'] == 0 and off['components']['av1_afbc']['effective'] == 0, 'new core independent rollback')
            set_flag('codec2_sdk_next', True)
            off = set_flag('all', False)
            expect(all(v['effective'] == 0 and v['requested'] == 1 for v in off['components'].values()), 'whole rollback retains all selections')
            expect(off['activeEnabled'] and off['pendingReboot'], 'whole disable never claims immediate rollback')
            set_flag('all', True)
            shell('sh ' + current + '/action.sh')
            expect(not status()['enabled'], 'original Action disables')
            shell('sh ' + current + '/action.sh')
            expect(status()['enabled'], 'original Action enables')
            put(current + '/active-boot-id', 'previous-boot\n')
            set_flag('all', False)
            disabled = status()
            expect(disabled['activeKnown'] and not disabled['activeEnabled'] and not disabled['pendingReboot'], 'disabled boot ignores stale active config')
            set_flag('all', True)
            shell('cat /proc/sys/kernel/random/boot_id > ' + current + '/active-boot-id')
            put(current + '/config', config() + 'mpp=1\n')
            expect(not status()['valid'], 'duplicate config key rejected')
            expect(not set_flag('all', False)['enabled'], 'whole rollback works with invalid config')
            put(current + '/config', config())
            set_flag('all', True)
            for parts in [('set', 'unknown', 'on'), ('set', 'mpp; touch ' + fixture + '/injected', 'on'), ('set', 'mpp', '2'), ('set', 'mpp', 'on', 'extra'), ('status', 'extra')]:
                expect(call(*parts, check=False).returncode != 0, 'reject arbitrary input ' + parts[0])
            expect(shell('test ! -e ' + fixture + '/injected', check=False).returncode == 0, 'no command injection')
            shell('mkdir -p ' + fixture + '/modules_update; cp -R ' + current + ' ' + staged)
            put(staged + '/module.prop', 'id=rk3588_decode_stack\nversion=0.6.5\n')
            expect(status()['staged'] and status()['pendingReboot'] and status()['version'] == '0.6.5', 'staged update visible')
            off = set_flag('av1_afbc', False)
            expect(off['components']['av1_afbc']['requested'] == 0, 'writes staged selection')
            expect('av1_afbc=0' in shell('cat ' + current + '/config').stdout, 'pending selection synchronized to current config')
            set_flag('av1_afbc', True)
            set_flag('all', False)
            expect(shell('test -f ' + staged + '/disable -a -f ' + current + '/disable', check=False).returncode == 0, 'whole rollback synchronized across pending update')
            set_flag('all', True)
            put(current + '/remove', '')
            expect(status()['removing'] and call('set', 'all', 'on', check=False).returncode != 0, 'uninstall marker blocks writes')
            expect(shell('cat ' + current + '/active-config').stdout == original_active, 'no test mutation touches active config')
        finally:
            # Fixed absolute disposable root; never a computed user path.
            shell('rm -rf /data/local/tmp/rk-webui-test064')
    print(json.dumps({'android_shell_assertions': assertions, 'live_module_modified': False, 'passed': True}))


if __name__ == '__main__':
    main()
