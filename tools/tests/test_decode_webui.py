"""Exercise real Android sh in an isolated fixture; never mount or edit the live module.

Usage: python tools/tests/test_decode_webui.py --adb /path/to/adb
Only fixed module-directory literals are redirected into the disposable fixture.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--bundles-only', action='store_true', help='Only test atomic grouped selections and preserved dependencies')
    parser.add_argument('--display-policy-only', action='store_true', help='Only test system fixed-mode/locale preflight and independent rollback controls')
    parser.add_argument('--qms-only', action='store_true', help='Only test cold QMS prerequisites, state truth and independent reboot rollback')
    parser.add_argument('--hwc-reuse-only', action='store_true', help='Only exercise the new HWC metadata dependency and rollback controls')
    parser.add_argument('--p010-pairs-only', action='store_true', help='Only test P010 pair unpack controls')
    parser.add_argument('--hevc-eos-only', action='store_true', help='Only test HEVC EOS input completion controls')
    parser.add_argument('--hevc-native-only', action='store_true', help='Only test native HEVC dependencies and reboot status')
    parser.add_argument('--gralloc-lease-only', action='store_true', help='Only exercise the new Gralloc HDR transfer controls')
    parser.add_argument('--hwui-only', action='store_true', help='Only test HWUI mapping controls')
    parser.add_argument('--p010-pool-only', action='store_true', help='Only test ARM64 P010 copy pool dependencies and reboot status')
    parser.add_argument('--hdmi-event-only', action='store_true', help='Only test HDMI event item, preflight and reboot status')
    parser.add_argument('--native-backpressure-only', action='store_true', help='Only test native backpressure dependencies and boot rollback')
    parser.add_argument('--surface-route-only', action='store_true', help='Only test native Surface rollback without disabling explicit P010 paths')
    parser.add_argument('--client-hdr-only', action='store_true', help='Only test client HDR controls')
    parser.add_argument('--mode-cache-only', action='store_true', help='Only test reported-mode cache controls')
    parser.add_argument('--scaled-overlay-only', action='store_true', help='Only test scaled-overlay dependencies and reboot rollback controls')
    parser.add_argument('--delay-notify-only', action='store_true', help='Only exercise output-delay notification controls')
    parser.add_argument('--lifecycle-only', action='store_true', help='Only exercise the new lifecycle fix controls')
    parser.add_argument('--texture-afbc-only', action='store_true', help='Only exercise the new texture AFBC dependency and rollback controls')
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / 'magisk-decode'
    fixture = '/data/local/tmp/rk-webui-test070'
    current = fixture + '/modules/rk3588_decode_stack'
    staged = fixture + '/modules_update/rk3588_decode_stack'
    keys = 'mpp rga codec2 libyuv software hdr_static hdr10plus codec2_sdk codec2_buffer p010_fast codec2_sdk_next av1_native av1_grain av1_afbc av1_texture_afbc codec2_lifecycle hwc_metadata_reuse hwc_scaled_overlay gralloc_hdr_lease codec2_delay_notify hevc_native_surface hevc_eos_pending p010_pairs hwc_mode_cache hwc_client_hdr10 hwui_rgb10 p010_copy_pool codec2_surface codec2_native_backpressure hdmi_audio_guard'.split()
    keys.extend(['display_qms','display_policy','display_session'])
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
        return ''.join(k + '=' + str(changes.get(k, 0 if k in {'display_qms','display_policy'} else 1)) + '\n' for k in keys)

    expect(shell('test ! -e ' + fixture, check=False).returncode == 0, 'fresh fixture required')
    with tempfile.TemporaryDirectory() as directory:
        temp = type('Temp', (), {'name': directory})()
        try:
            shell('mkdir -p ' + current + '/components/codec2_sdk/system/vendor/lib/hw ' + current + '/components/codec2_sdk_next/system/vendor/lib/hw')
            for name in ['common.sh', 'mode-control.sh', 'selection.sh', 'bundles.sh', 'action.sh', 'webui.sh', 'module.prop']:
                data = (source / name).read_text(encoding='utf-8')
                data = data.replace('/data/adb/modules_update', fixture + '/modules_update').replace('/data/adb/modules', fixture + '/modules')
                data = data.replace('/proc/cmdline', fixture + '/cmdline')
                data = data.replace('/sys/module/rk_decode_qms/parameters/owner_online', fixture + '/qms-owner')
                data = data.replace('$(getprop vendor.rkhdr.qms_boot)', '$(cat ' + fixture + '/qms-boot)')
                data = data.replace('$(getprop vendor.rkhdr.qms)', '$(cat ' + fixture + '/qms-hwc)')
                data = data.replace('$(getprop vendor.gralloc.no_afbc_for_fb_target_layer)', '$(cat ' + fixture + '/fb-linear)')
                put(current + '/' + name, data)
            for component in ['codec2_sdk', 'codec2_sdk_next']:
                put(current + '/components/' + component + '/system/vendor/lib/hw/libcodec2_rx_component.so', 'fixture-not-a-library')
            shell('mkdir -p ' + current + '/components/hdr')
            put(fixture + '/cmdline', 'initcall_blacklist=kbase_platform_driver_init\n')
            put(fixture + '/fb-linear', '0\n')
            put(current + '/components/hdr/sf-target10-preflight', 'fixture-only prerequisite, not an SF implementation')
            shell('mkdir -p ' + current + '/components/hdr/target')
            put(current + '/components/hdr/target/libsfhdr.so', 'fixture-only-not-an-ELF')
            put(current + '/components/hdr/target/libga10.so', 'fixture-only-not-an-ELF')
            put(current + '/components/hdr/alloc-target10-preflight', 'fixture-only prerequisite')
            shell('mkdir -p ' + current + '/components/codec2_pool/system/lib64')
            put(current + '/components/codec2_pool/system/lib64/libcp10.so', 'fixture-only-not-an-ELF')
            shell('mkdir -p ' + current + '/components/hdmi_event/target')
            put(current + '/components/hdmi_event/target/libhevt.so', 'fixture-only-not-an-ELF')
            put(current + '/components/hdmi_event/preflight', 'fixture-only')
            put(current + '/config', config())
            unknown = status()
            expect(not unknown['activeKnown'] and unknown['pendingReboot'], 'legacy/first install never claims active')
            put(current + '/active-config', config())
            version = next(line.split('=', 1)[1] for line in (source / 'module.prop').read_text(encoding='utf-8').splitlines() if line.startswith('version='))
            put(current + '/active-version', version + '\n')
            shell('cat /proc/sys/kernel/random/boot_id > ' + current + '/active-boot-id')
            put(current + '/status.txt', 'PASS unified decode stack, fixture only\n')
            ready = status()
            expect(ready['activeKnown'] and ready['activeEnabled'] and not ready['pendingReboot'], 'matching boot receipt')
            expect(ready['health'] == 'passed', 'bounded health status')
            original_active = shell('cat ' + current + '/active-config').stdout
            if args.bundles_only:
                import re
                groups = dict(re.findall(r"^ (\w+)\) echo '([^']+)'", (source/'bundles.sh').read_text(), re.M))
                expect(set(groups)=={'core','native','av1','hdr','display'},'fixed group allowlist')
                for group, members in groups.items():
                    before=status()
                    s=json.loads(call('bundle',group,'off').stdout)
                    expect(all(s['components'][k]['requested']==0 for k in members.split()),group+' off changes all members')
                    expect(all(s['components'][k]['requested']==v['requested'] for k,v in before['components'].items() if k not in members.split()),group+' off preserves other selections')
                    s=json.loads(call('bundle',group,'on').stdout)
                    expect(all(s['components'][k]['requested']==1 for k in members.split()),group+' on selects members')
                    expect(s['components']['display_qms']['requested']==0,group+' never selects QMS')
                    expect(shell('cat '+current+'/active-config').stdout==original_active,'no live switch')
                set_flag('codec2_sdk_next',False)
                s=json.loads(call('bundle','native','on').stdout)
                expect(s['components']['codec2_sdk_next']['requested']==1,'native bundle includes core dependency')
                before=shell('cat '+current+'/config').stdout
                for args_bad in [('bundle','qms','on'),('bundle','core;reboot','on'),('bundle','core','oops'),('bundle','core','on','extra')]:
                    expect(call(*args_bad,check=False).returncode==2,'reject invalid bundle command')
                    expect(shell('cat '+current+'/config').stdout==before,'rejection preserves config')
                shell('mkdir -p '+staged)
                shell('cp -R '+current+'/. '+staged+'/')
                put(staged+'/config',config())
                s=json.loads(call('bundle','native','off').stdout)
                expect(s['staged'] and s['components']['codec2_surface']['requested']==0,'edits staged update')
                expect(shell('cat '+current+'/config').stdout==shell('cat '+staged+'/config').stdout,'staged selections mirrored')
                print(json.dumps({'bundle_shell_assertions':assertions,'live_module_modified':False,'passed':True}))
                return
            if args.display_policy_only:
                s=set_flag('display_policy',True)
                expect(s['components']['display_policy']['requested']==1 and s['components']['display_policy']['effective']==0,'missing policy preflight cannot activate')
                shell('mkdir -p '+current+'/components/display_policy/variants')
                put(current+'/components/display_policy/preflight','fixture-only')
                put(current+'/components/display_policy/variants/services.jar','fixture-only-not-a-framework')
                put(current+'/components/display_policy/oat.sha256','fixture-only-not-an-OAT')
                put(current+'/components/display_policy/oat-inputs.sha256','fixture-only-not-a-runtime-contract')
                s=status()
                expect(s['components']['display_policy']['effective']==1 and s['components']['display_policy']['active']==0 and s['pendingReboot'],'prepared policy remains pending until reboot')
                expect(s['components']['display_qms']['requested']==0,'policy does not enable QMS')
                put(current+'/active-config',config(display_policy=1))
                expect(not status()['pendingReboot'],'simulated current boot activation')
                s=set_flag('display_policy',False)
                expect(s['components']['display_policy']['active']==1 and s['components']['display_policy']['effective']==0 and s['pendingReboot'],'policy disable requires reboot, no false live restore')
                expect(s['components']['codec2']['effective']==1 and s['components']['hdr_static']['effective']==1,'policy rollback retains decode and HDR')
                s=set_flag('all',False)
                expect(not s['enabled'] and s['components']['display_policy']['effective']==0,'whole module rollback includes system policy')
                print(json.dumps({'display_policy_shell_assertions':assertions,'live_module_modified':False,'passed':True}))
                return
            if args.qms_only:
                shell('mkdir -p ' + current + '/components/qms')
                for name in ['profile', 'rk_decode_qms.ko', 'boot-stage64']:
                    put(current + '/components/qms/' + name, 'fixture-only-not-a-payload')
                shell('chmod 755 ' + current + '/components/qms/boot-stage64')
                s = set_flag('display_qms', True)
                expect(s['components']['display_qms']['effective'] == 1 and s['pendingReboot'], 'cold QMS queued, never late-bound')
                expect(s['components']['display_qms']['active'] == 0, 'no false live activation')
                for name in ['profile', 'rk_decode_qms.ko', 'boot-stage64']:
                    path = current + '/components/qms/' + name
                    shell('mv ' + path + ' ' + path + '.saved')
                    expect(status()['components']['display_qms']['effective'] == 0, 'missing cold prerequisite ' + name)
                    shell('mv ' + path + '.saved ' + path)
                set_flag('hdr_static', False)
                expect(status()['components']['display_qms']['effective'] == 0, 'HDR dependency retained')
                set_flag('display_qms', True)
                expect(status()['components']['hdr_static']['effective'] == 1, 'enable QMS plans its dependency')
                put(current + '/active-config', config(display_qms=1))
                expect(status()['components']['display_qms']['active'] == 0, 'config alone cannot claim owner binding')
                put(fixture + '/qms-owner', '1\n');put(fixture + '/qms-hwc', 'active\n')
                shell('cat /proc/sys/kernel/random/boot_id > ' + fixture + '/qms-boot')
                expect(status()['components']['display_qms']['active'] == 1, 'actual current owner plus HWC receipts required')
                off = set_flag('display_qms', False)
                expect(off['components']['display_qms']['active'] == 1 and off['components']['display_qms']['effective'] == 0 and off['pendingReboot'], 'QMS removal is reboot-only')
                for key in ['codec2_native_backpressure', 'codec2_surface', 'hwc_client_hdr10']:
                    expect(off['components'][key]['effective'] == 1, 'QMS rollback preserves ' + key)
                put(current+'/assemble.sh','#!/system/bin/sh\nexit 0\n')
                put(fixture+'/resetprop','#!/system/bin/sh\nexit 0\n')
                shell('chmod 755 '+fixture+'/resetprop')
                put(current+'/active-config',config(display_qms=1))
                data=(source/'post-fs-data.sh').read_text(encoding='utf8').replace('resetprop ',fixture+'/resetprop ')
                data=data.replace('/data/adb/rk3588-decode-qms',fixture+'/display-state')
                put(current+'/post-fs-data.sh',data)
                shell('umask 000; sh '+current+'/post-fs-data.sh')
                expect(shell('stat -c %a '+current+'/active-boot-id').stdout.strip()=='600','boot receipt ignores permissive init umask')
                expect(shell('stat -c %a '+fixture+'/display-state/plan').stdout.strip()=='600','display plan stays private')
                print(json.dumps({'qms_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.native_backpressure_only:
                put(current + '/config', config(codec2_native_backpressure=0, codec2_surface=0, codec2_lifecycle=0, codec2_sdk_next=0))
                linked = set_flag('codec2_native_backpressure', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'codec2_surface', 'codec2_lifecycle', 'codec2_native_backpressure']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'enable dependency ' + key)
                off = set_flag('codec2_native_backpressure', False)
                expect(off['components']['codec2_native_backpressure']['effective'] == 0 and off['pendingReboot'], 'reboot-only candidate rollback')
                for key in ['codec2_surface', 'codec2_sdk_next', 'codec2_lifecycle', 'p010_copy_pool', 'hwc_client_hdr10']:
                    expect(off['components'][key]['effective'] == 1, 'candidate off preserves ' + key)
                for key in ['codec2_surface', 'codec2_sdk_next', 'codec2_lifecycle']:
                    set_flag('codec2_native_backpressure', True)
                    off = set_flag(key, False)
                    expect(off['components']['codec2_native_backpressure']['requested'] == 1 and off['components']['codec2_native_backpressure']['effective'] == 0, 'dependency suppression ' + key)
                    set_flag(key, True)
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'no live core handover')
                off = json.loads(call('set', 'all', 'off').stdout)
                expect(off['components']['codec2_native_backpressure']['effective'] == 0, 'whole module rollback')
                expect(call('set', 'codec2_native_backpressure;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                put(current + '/assemble.sh', '#!/system/bin/sh\nexit 0\n')
                put(fixture + '/resetprop', '#!/system/bin/sh\nprintf "%s\\n" "$*" >> ' + fixture + '/property-calls\n')
                shell('chmod 755 ' + fixture + '/resetprop')
                data = (source / 'post-fs-data.sh').read_text(encoding='utf-8').replace('resetprop ', fixture + '/resetprop ')
                put(current + '/post-fs-data.sh', data)
                for enabled in [1, 0]:
                    put(current + '/active-config', config(codec2_native_backpressure=enabled))
                    shell('rm -f ' + fixture + '/property-calls; sh ' + current + '/post-fs-data.sh')
                    properties = shell('cat ' + fixture + '/property-calls').stdout.splitlines()
                    for expected in ['-n debug.rkcodec2.native_backpressure ' + str(enabled), '-n debug.rkcodec2.surface_upgrade 1', '-n debug.rkcodec2.native_surface_upgrade 1', '-n debug.rkcodec2.p010_pool 1']:
                        expect(expected in properties, 'independent boot property: ' + expected)
                print(json.dumps({'native_backpressure_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.surface_route_only:
                off = set_flag('codec2_surface', False)
                expect(off['components']['codec2_surface']['effective'] == 0, 'native Surface rollback selected')
                expect(off['components']['codec2_surface']['active'] == 1 and off['pendingReboot'], 'requires reboot, no live handover')
                for key in ['codec2', 'codec2_sdk_next', 'codec2_buffer', 'p010_copy_pool', 'p010_pairs', 'hdr_static']:
                    expect(off['components'][key]['effective'] == 1, 'rollback preserves ' + key)
                for key in ['av1_native', 'av1_afbc', 'av1_texture_afbc', 'hevc_native_surface']:
                    expect(off['components'][key]['requested'] == 1 and off['components'][key]['effective'] == 0, 'native-only dependency suppressed ' + key)
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt unchanged before reboot')
                for key in ['av1_native', 'hevc_native_surface']:
                    linked = set_flag(key, True)
                    expect(linked['components']['codec2_surface']['requested'] == linked['components']['codec2_surface']['effective'] == 1, 'native feature explicitly enables Surface prerequisite ' + key)
                    set_flag('codec2_surface', False)
                set_flag('codec2', False)
                linked = set_flag('codec2_surface', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_surface']:
                    expect(linked['components'][key]['effective'] == 1, 'Surface selection enables ' + key)
                all_off = json.loads(call('set', 'all', 'off').stdout)
                expect(all_off['components']['codec2_surface']['effective'] == 0, 'whole rollback overrides route')
                expect(call('set', 'codec2_surface;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                # Execute the real boot-property script against inert stubs.
                # The firmware P010 gate must stay on during native-only rollback.
                put(current + '/assemble.sh', '#!/system/bin/sh\nexit 0\n')
                put(fixture + '/resetprop', '#!/system/bin/sh\nprintf "%s\\n" "$*" >> ' + fixture + '/property-calls\n')
                shell('chmod 755 ' + fixture + '/resetprop')
                put(current + '/active-config', config(codec2_surface=0))
                data = (source / 'post-fs-data.sh').read_text(encoding='utf-8').replace('resetprop ', fixture + '/resetprop ')
                put(current + '/post-fs-data.sh', data)
                shell('sh ' + current + '/post-fs-data.sh')
                properties = shell('cat ' + fixture + '/property-calls').stdout.splitlines()
                for expected in ['-n debug.rkcodec2.surface_upgrade 1', '-n debug.rkcodec2.native_surface_upgrade 0', '-n debug.rkcodec2.p010_pool 1']:
                    expect(expected in properties, 'independent P010/native boot property: ' + expected)
                print(json.dumps({'surface_route_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.hdmi_event_only:
                put(current + '/config', config(hdmi_audio_guard=0))
                s = set_flag('hdmi_audio_guard', True)
                expect(s['components']['hdmi_audio_guard']['effective'] == 1, 'explicit HDMI selection')
                for prerequisite in ['preflight', 'target/libhevt.so']:
                    path = current + '/components/hdmi_event/' + prerequisite
                    shell('mv ' + path + ' ' + path + '.saved')
                    s = status()
                    expect(s['components']['hdmi_audio_guard']['requested'] == 1 and s['components']['hdmi_audio_guard']['effective'] == 0, 'missing prerequisite suppresses HDMI ' + prerequisite)
                    shell('mv ' + path + '.saved ' + path)
                off = set_flag('hdmi_audio_guard', False)
                expect(off['components']['hdmi_audio_guard']['active'] == 1 and off['components']['hdmi_audio_guard']['effective'] == 0 and off['pendingReboot'], 'no live framework change')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'receipt unchanged')
                all_off = json.loads(call('set', 'all', 'off').stdout)
                expect(not all_off['enabled'] and all_off['components']['hdmi_audio_guard']['effective'] == 0, 'whole-off suppresses HDMI overlay')
                expect(call('set', 'hdmi_audio_guard;id', 'on', check=False).returncode != 0, 'fixed argument boundary')
                print(json.dumps({'HDMI_event_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.p010_pool_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, codec2_buffer=0, p010_copy_pool=0))
                linked = set_flag('p010_copy_pool', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'codec2_buffer', 'p010_copy_pool']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'P010 pool enables ' + key)
                for parent in ['codec2_sdk_next', 'codec2_buffer']:
                    off = set_flag(parent, False)
                    expect(off['components']['p010_copy_pool']['requested'] == 1 and off['components']['p010_copy_pool']['effective'] == 0, 'missing ' + parent + ' suppresses pool without forgetting choice')
                    set_flag('p010_copy_pool', True)
                shell('rm ' + current + '/components/codec2_pool/system/lib64/libcp10.so')
                expect(status()['components']['p010_copy_pool']['effective'] == 0, 'missing component cannot claim pool activation')
                put(current + '/components/codec2_pool/system/lib64/libcp10.so', 'fixture-only-not-an-ELF')
                off = set_flag('p010_copy_pool', False)
                expect(off['components']['codec2_sdk_next']['effective'] == 1 and off['components']['codec2_buffer']['effective'] == 1, 'independent rollback retains parents')
                expect(off['components']['p010_copy_pool']['active'] == 1 and off['pendingReboot'], 'no live format change')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'boot receipt unchanged')
                expect(call('set', 'p010_copy_pool;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'p010_pool_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.client_hdr_only:
                put(fixture + '/fb-linear', '1\n')
                expect(status()['components']['hwc_client_hdr10']['effective'] == 0, 'linear FB policy cannot activate unscannable RGB10')
                put(fixture + '/fb-linear', '0\n')
                expect(status()['components']['hwc_client_hdr10']['effective'] == 1, 'original AFBC policy permits paired HDR candidate')
                shell('rm ' + current + '/components/hdr/sf-target10-preflight')
                unavailable = status()
                expect(unavailable['components']['hwc_client_hdr10']['requested'] == 1 and unavailable['components']['hwc_client_hdr10']['effective'] == 0, 'missing validated SF target gates incomplete HDR inactive')
                put(current + '/components/hdr/sf-target10-preflight', 'fixture-only prerequisite')
                shell('rm ' + current + '/components/hdr/alloc-target10-preflight')
                expect(status()['components']['hwc_client_hdr10']['effective'] == 0, 'missing allocator prerequisite prevents partial HDR')
                put(current + '/components/hdr/alloc-target10-preflight', 'fixture-only prerequisite')
                shell('rm ' + current + '/components/hdr/target/libga10.so')
                expect(status()['components']['hwc_client_hdr10']['effective'] == 0, 'missing allocator adapter prevents partial HDR')
                put(current + '/components/hdr/target/libga10.so', 'fixture-only-not-an-ELF')
                put(fixture + '/cmdline', 'stock-kernel\n')
                expect(status()['components']['hwc_client_hdr10']['effective'] == 0, 'missing first-binding barrier cannot schedule a late allocator swap')
                put(fixture + '/cmdline', 'initcall_blacklist=kbase_platform_driver_init\n')
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, hwc_client_hdr10=0))
                linked = set_flag('hwc_client_hdr10', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'hwui_rgb10', 'hwc_client_hdr10']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'overlay enables ' + key)
                off = set_flag('hdr_static', False)
                expect(off['components']['hwc_client_hdr10']['requested'] == 1 and off['components']['hwc_client_hdr10']['effective'] == 0, 'parent off retains inactive overlay selection')
                set_flag('hwc_client_hdr10', True)
                off = set_flag('hwc_client_hdr10', False)
                expect(off['components']['hdr_static']['effective'] == 1, 'independent off retains HDR adapter')
                expect(off['components']['hwc_client_hdr10']['active'] == 1 and off['pendingReboot'], 'no live policy change')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'boot receipt unchanged')
                expect(call('set', 'hwc_client_hdr10;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'client_hdr_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.hwui_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, hwui_rgb10=0))
                linked = set_flag('hwui_rgb10', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'hwui_rgb10']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'overlay enables ' + key)
                off = set_flag('hdr_static', False)
                expect(off['components']['hwui_rgb10']['requested'] == 1 and off['components']['hwui_rgb10']['effective'] == 0, 'parent off retains inactive overlay selection')
                set_flag('hwui_rgb10', True)
                off = set_flag('hwui_rgb10', False)
                expect(off['components']['hdr_static']['effective'] == 1, 'independent off retains HDR adapter')
                expect(off['components']['hwui_rgb10']['active'] == 1 and off['pendingReboot'], 'no live policy change')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'boot receipt unchanged')
                expect(call('set', 'hwui_rgb10;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'hwui_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.mode_cache_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, hwc_mode_cache=0))
                linked = set_flag('hwc_mode_cache', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'hwc_mode_cache']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'overlay enables ' + key)
                off = set_flag('hdr_static', False)
                expect(off['components']['hwc_mode_cache']['requested'] == 1 and off['components']['hwc_mode_cache']['effective'] == 0, 'parent off retains inactive overlay selection')
                set_flag('hwc_mode_cache', True)
                off = set_flag('hwc_mode_cache', False)
                expect(off['components']['hdr_static']['effective'] == 1, 'independent off retains HDR adapter')
                expect(off['components']['hwc_mode_cache']['active'] == 1 and off['pendingReboot'], 'no live policy change')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'boot receipt unchanged')
                expect(call('set', 'hwc_mode_cache;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'mode_cache_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.scaled_overlay_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, hwc_scaled_overlay=0))
                linked = set_flag('hwc_scaled_overlay', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'hwc_scaled_overlay']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'overlay enables ' + key)
                off = set_flag('hdr_static', False)
                expect(off['components']['hwc_scaled_overlay']['requested'] == 1 and off['components']['hwc_scaled_overlay']['effective'] == 0, 'parent off retains inactive overlay selection')
                set_flag('hwc_scaled_overlay', True)
                off = set_flag('hwc_scaled_overlay', False)
                expect(off['components']['hdr_static']['effective'] == 1, 'independent off retains HDR adapter')
                expect(off['components']['hwc_scaled_overlay']['active'] == 1 and off['pendingReboot'], 'no live policy change')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'boot receipt unchanged')
                expect(call('set', 'hwc_scaled_overlay;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'scaled_overlay_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.p010_pairs_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, p010_pairs=0))
                linked = set_flag('p010_pairs', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'p010_fast', 'p010_pairs']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'delay pair unpack links ' + key)
                off = set_flag('codec2_sdk_next', False)
                expect(off['components']['p010_pairs']['requested'] == 1 and off['components']['p010_pairs']['effective'] == 0, 'core suppression retains pair unpack choice')
                set_flag('p010_pairs', True)
                off = set_flag('p010_pairs', False)
                expect(off['components']['codec2_sdk_next']['effective'] == 1 and off['components']['gralloc_hdr_lease']['effective'] == 1, 'independent rollback retains prior core and transfer')
                expect(off['components']['p010_pairs']['active'] == 1 and off['pendingReboot'], 'saved pair unpack changes require reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt unchanged')
                expect(call('set', 'p010_pairs;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                set_flag('p010_pairs', True)
                off = set_flag('p010_fast', False)
                expect(off['components']['p010_pairs']['requested'] == 1 and off['components']['p010_pairs']['effective'] == 0, 'P010 acceleration dependency suppresses pair mode')
                linked = set_flag('p010_pairs', True)
                expect(linked['components']['p010_fast']['effective'] == 1, 'linked pair mode restores P010 acceleration')
                print(json.dumps({'p010_pairs_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.delay_notify_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, codec2_delay_notify=0))
                linked = set_flag('codec2_delay_notify', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'codec2_delay_notify']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'delay notification links ' + key)
                off = set_flag('codec2_sdk_next', False)
                expect(off['components']['codec2_delay_notify']['requested'] == 1 and off['components']['codec2_delay_notify']['effective'] == 0, 'core suppression retains notification choice')
                set_flag('codec2_delay_notify', True)
                off = set_flag('codec2_delay_notify', False)
                expect(off['components']['codec2_sdk_next']['effective'] == 1 and off['components']['gralloc_hdr_lease']['effective'] == 1, 'independent rollback retains prior core and transfer')
                expect(off['components']['codec2_delay_notify']['active'] == 1 and off['pendingReboot'], 'saved notification changes require reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt unchanged')
                expect(call('set', 'codec2_delay_notify;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'new_delay_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.hevc_eos_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, hevc_eos_pending=0))
                linked = set_flag('hevc_eos_pending', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'hevc_eos_pending']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'delay EOS return links ' + key)
                off = set_flag('codec2_sdk_next', False)
                expect(off['components']['hevc_eos_pending']['requested'] == 1 and off['components']['hevc_eos_pending']['effective'] == 0, 'core suppression retains EOS return choice')
                set_flag('hevc_eos_pending', True)
                off = set_flag('hevc_eos_pending', False)
                expect(off['components']['codec2_sdk_next']['effective'] == 1 and off['components']['gralloc_hdr_lease']['effective'] == 1, 'independent rollback retains prior core and transfer')
                expect(off['components']['hevc_eos_pending']['active'] == 1 and off['pendingReboot'], 'saved EOS return changes require reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt unchanged')
                expect(call('set', 'hevc_eos_pending;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'hevc_eos_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.delay_notify_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, codec2_delay_notify=0))
                linked = set_flag('codec2_delay_notify', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'codec2_delay_notify']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'delay notification links ' + key)
                off = set_flag('codec2_sdk_next', False)
                expect(off['components']['codec2_delay_notify']['requested'] == 1 and off['components']['codec2_delay_notify']['effective'] == 0, 'core suppression retains notification choice')
                set_flag('codec2_delay_notify', True)
                off = set_flag('codec2_delay_notify', False)
                expect(off['components']['codec2_sdk_next']['effective'] == 1 and off['components']['gralloc_hdr_lease']['effective'] == 1, 'independent rollback retains prior core and transfer')
                expect(off['components']['codec2_delay_notify']['active'] == 1 and off['pendingReboot'], 'saved notification changes require reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt unchanged')
                expect(call('set', 'codec2_delay_notify;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'new_delay_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.hevc_native_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, codec2_sdk=0, codec2_sdk_next=0, hevc_native_surface=0))
                linked = set_flag('hevc_native_surface', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'codec2_sdk', 'codec2_sdk_next', 'hevc_native_surface']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'HEVC native links ' + key)
                for parent in ['hdr_static', 'codec2_sdk_next']:
                    off = set_flag(parent, False)
                    expect(off['components']['hevc_native_surface']['requested'] == 1 and off['components']['hevc_native_surface']['effective'] == 0, parent + ' suppresses retained native choice')
                    set_flag('hevc_native_surface', True)
                off = set_flag('hevc_native_surface', False)
                expect(off['components']['hdr_static']['effective'] == off['components']['codec2_sdk_next']['effective'] == 1, 'independent rollback retains HDR and core')
                expect(off['components']['hevc_native_surface']['active'] == 1 and off['pendingReboot'], 'selection never activates live')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt untouched')
                expect(call('set', 'hevc_native_surface;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'hevc_native_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.gralloc_lease_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, codec2_sdk=0, codec2_sdk_next=0, gralloc_hdr_lease=0))
                linked = set_flag('gralloc_hdr_lease', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'codec2_sdk', 'codec2_sdk_next', 'gralloc_hdr_lease']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'Gralloc transfer links ' + key)
                for parent in ['hdr_static', 'codec2_sdk_next']:
                    off = set_flag(parent, False)
                    expect(off['components']['gralloc_hdr_lease']['requested'] == 1 and off['components']['gralloc_hdr_lease']['effective'] == 0, parent + ' suppresses retained transfer choice')
                    set_flag('gralloc_hdr_lease', True)
                off = set_flag('gralloc_hdr_lease', False)
                expect(off['components']['hdr_static']['effective'] == off['components']['codec2_sdk_next']['effective'] == 1, 'independent rollback retains HDR and core')
                expect(off['components']['gralloc_hdr_lease']['active'] == 1 and off['pendingReboot'], 'selection never activates live')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'active receipt untouched')
                expect(call('set', 'gralloc_hdr_lease;id', 'on', check=False).returncode != 0, 'fixed command boundary')
                print(json.dumps({'new_gralloc_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.hwc_reuse_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, hdr_static=0, hwc_metadata_reuse=0))
                linked = set_flag('hwc_metadata_reuse', True)
                for key in ['mpp', 'rga', 'codec2', 'hdr_static', 'hwc_metadata_reuse']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'HWC links ' + key)
                off = set_flag('hdr_static', False)
                expect(off['components']['hwc_metadata_reuse']['requested'] == 1 and off['components']['hwc_metadata_reuse']['effective'] == 0, 'HDR dependency preserves saved HWC choice')
                restored = set_flag('hwc_metadata_reuse', True)
                expect(restored['components']['hdr_static']['effective'] == 1, 'HWC restores HDR dependency')
                off = set_flag('hwc_metadata_reuse', False)
                expect(off['components']['hdr_static']['effective'] == 1, 'HWC rollback retains prior HDR bridge')
                expect(off['components']['hwc_metadata_reuse']['active'] == 1 and off['pendingReboot'], 'HWC save requires reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'HWC fixture never changes active receipt')
                print(json.dumps({'new_hwc_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.lifecycle_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, codec2_lifecycle=0))
                linked = set_flag('codec2_lifecycle', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'codec2_lifecycle']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'lifecycle links ' + key)
                off = set_flag('codec2_sdk_next', False)
                expect(off['components']['codec2_lifecycle']['requested'] == 1 and off['components']['codec2_lifecycle']['effective'] == 0, 'backend suppression preserves lifecycle choice')
                restored = set_flag('codec2_lifecycle', True)
                expect(restored['components']['codec2_sdk_next']['effective'] == 1, 'lifecycle restores latest backend')
                off = set_flag('codec2_lifecycle', False)
                expect(off['components']['codec2_sdk_next']['effective'] == 1 and off['components']['av1_texture_afbc']['effective'] == 1, 'independent lifecycle rollback retains previous features')
                expect(off['components']['codec2_lifecycle']['active'] == 1 and off['pendingReboot'], 'lifecycle save requires reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'lifecycle fixture never changes active receipt')
                print(json.dumps({'new_lifecycle_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
            if args.texture_afbc_only:
                put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0,
                                               codec2_sdk_next=0, av1_native=0, av1_afbc=0,
                                               av1_texture_afbc=0))
                linked = set_flag('av1_texture_afbc', True)
                for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'av1_native', 'av1_afbc', 'av1_texture_afbc']:
                    expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1,
                           'texture AFBC recursively enables ' + key)
                off = set_flag('av1_afbc', False)
                expect(off['components']['av1_texture_afbc']['requested'] == 1 and off['components']['av1_texture_afbc']['effective'] == 0,
                       'parent suppression retains requested texture mode')
                restored = set_flag('av1_texture_afbc', True)
                expect(restored['components']['av1_afbc']['effective'] == 1, 'enabling child restores parent')
                off = set_flag('av1_texture_afbc', False)
                expect(off['components']['av1_afbc']['effective'] == 1 and off['components']['av1_native']['effective'] == 1,
                       'independent texture rollback preserves native/composer paths')
                expect(off['components']['av1_texture_afbc']['active'] == 1 and off['pendingReboot'], 'rollback requires reboot')
                expect(shell('cat ' + current + '/active-config').stdout == original_active, 'live boot receipt unchanged')
                print(json.dumps({'new_texture_control_assertions': assertions, 'live_module_modified': False, 'passed': True}))
                return
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
            for name, sdk, latest in [('legacy', 0, 0), ('simd', 1, 0), ('latest', 1, 1)]:
                selected = json.loads(call('backend', name).stdout)
                expect(selected['components']['codec2_sdk']['requested'] == sdk and selected['components']['codec2_sdk_next']['requested'] == latest, name + ' backend committed as a pair')
                expect(selected['components']['av1_afbc']['requested'] == 1, name + ' retains dependent choice')
                expect(selected['components']['av1_afbc']['effective'] == latest, name + ' gates AV1 native route')
            put(current + '/config', config(mpp=0, rga=0, codec2=0, codec2_sdk=0, codec2_sdk_next=0, av1_native=0, av1_afbc=0))
            linked = set_flag('av1_afbc', True)
            for key in ['mpp', 'rga', 'codec2', 'codec2_sdk', 'codec2_sdk_next', 'av1_native', 'av1_afbc']:
                expect(linked['components'][key]['requested'] == linked['components'][key]['effective'] == 1, 'enable AV1 AFBC recursively enables ' + key)
            expect(not linked['pendingReboot'], 'restoring full dependency plan cancels pending changes')
            off = set_flag('av1_afbc', False)
            expect(off['components']['av1_texture_afbc']['requested'] == 1 and off['components']['av1_texture_afbc']['effective'] == 0, 'texture AFBC suppressed with parent and choice retained')
            restored = set_flag('av1_texture_afbc', True)
            expect(restored['components']['av1_afbc']['requested'] == 1 and restored['components']['av1_texture_afbc']['effective'] == 1, 'texture AFBC enables compressed parent')
            off = set_flag('av1_texture_afbc', False)
            expect(off['components']['av1_afbc']['effective'] == 1 and off['components']['av1_native']['effective'] == 1, 'texture rollback preserves established composer and P010 routes')
            set_flag('av1_texture_afbc', True)
            set_flag('mpp', False)
            expect(set_flag('av1_grain', True)['components']['mpp']['requested'] == 1, 'enable grain restores MPP')
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
            for parts in [('set', 'unknown', 'on'), ('set', 'mpp; touch ' + fixture + '/injected', 'on'), ('set', 'mpp', '2'), ('set', 'mpp', 'on', 'extra'), ('status', 'extra'), ('backend', 'bad'), ('backend', 'latest', 'extra')]:
                expect(call(*parts, check=False).returncode != 0, 'reject arbitrary input ' + parts[0])
            expect(shell('test ! -e ' + fixture + '/injected', check=False).returncode == 0, 'no command injection')
            shell('mkdir -p ' + fixture + '/modules_update; cp -R ' + current + ' ' + staged)
            put(staged + '/module.prop', 'id=rk3588_decode_stack\nversion=0.6.5\n')
            expect(status()['staged'] and status()['pendingReboot'] and status()['version'] == '0.6.5', 'staged update visible')
            off = set_flag('av1_afbc', False)
            expect(off['components']['av1_afbc']['requested'] == 0, 'writes staged selection')
            expect('av1_afbc=0' in shell('cat ' + current + '/config').stdout, 'pending selection synchronized to current config')
            set_flag('av1_afbc', True)
            selected = json.loads(call('backend', 'simd').stdout)
            expect(selected['components']['codec2_sdk_next']['requested'] == 0, 'backend saved to pending update')
            expect('codec2_sdk_next=0' in shell('cat ' + current + '/config').stdout, 'backend synchronized to current config')
            call('backend', 'latest')
            set_flag('all', False)
            expect(shell('test -f ' + staged + '/disable -a -f ' + current + '/disable', check=False).returncode == 0, 'whole rollback synchronized across pending update')
            set_flag('all', True)
            put(current + '/remove', '')
            expect(status()['removing'] and call('set', 'all', 'on', check=False).returncode != 0, 'uninstall marker blocks writes')
            expect(shell('cat ' + current + '/active-config').stdout == original_active, 'no test mutation touches active config')
        finally:
            # Fixed absolute disposable root; never a computed user path.
            shell('rm -rf /data/local/tmp/rk-webui-test070')
    print(json.dumps({'android_shell_assertions': assertions, 'live_module_modified': False, 'passed': True}))


if __name__ == '__main__':
    main()
