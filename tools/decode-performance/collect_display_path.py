"""Collect timestamped RK3588 display-path evidence without changing driver policy.

Raw captures may contain application/media/device information. Keep them local.
"""
from pathlib import Path
import argparse
import datetime
import json
import re
import shlex
import subprocess
import time
import uuid


STATE = r'''
echo ===uptime; cat /proc/uptime
echo ===gpu; for n in governor cur_freq load; do echo "$n"; cat /sys/class/devfreq/fb000000.gpu/$n; done
echo ===dmc; for n in governor cur_freq; do echo "$n"; cat /sys/class/devfreq/dmc/$n; done
echo ===thermal; for z in /sys/class/thermal/thermal_zone*; do cat "$z/type" "$z/temp"; done
echo ===rga; cat /sys/kernel/debug/rkrga/load
echo ===drm; cat /sys/kernel/debug/dri/0/summary
echo ===mpp; cat /proc/mpp_service/sessions-summary
echo ===cpu; head -1 /proc/stat
'''


def parse_state(raw):
    sections = {}
    for part in raw.split('===')[1:]:
        name, _, body = part.partition('\n')
        sections[name.strip()] = body.strip()
    gpu = sections.get('gpu', '')
    freq = re.search(r'cur_freq\s+(\d+)', gpu)
    load = re.search(r'load\s+(\d+)@(\d+)Hz', gpu)
    return {
        'uptime': sections.get('uptime'),
        'gpu_governor': next(iter(re.findall(r'governor\s+(\S+)', gpu)), None),
        'gpu_freq_hz': int(freq[1]) if freq else None,
        'gpu_load_pct': int(load[1]) if load else None,
        'thermal': sections.get('thermal'),
        'rga_load': sections.get('rga'),
        'drm_planes': [line.strip() for line in sections.get('drm', '').splitlines()
                       if any(word in line for word in ('format:', 'src:', 'dst:', 'Display mode:', 'eotf:'))],
    }


def parse_displays(raw):
    """Keep physical and mirrored displays separate when interpreting CLIENT."""
    displays = []
    for block in raw.split('+ DisplayDevice{')[1:]:
        header = block.splitlines()[0]
        state = re.search(r'isEnabled=(true|false).*?usesClientComposition=(true|false)'
                          r'.*?usesDeviceComposition=(true|false)', block)
        displays.append({
            'virtual': bool(re.search(r'\bisVirtual=true\b', block)),
            'name': next(iter(re.findall(r'"([^"]*)"', header)), None),
            'enabled': state[1] == 'true' if state else None,
            'client_composition': state[2] == 'true' if state else None,
            'device_composition': state[3] == 'true' if state else None,
        })
    return displays


def parse_latency(raw):
    lines = raw.splitlines()
    period = int(lines[0]) if lines and lines[0].isdigit() else None
    present = set()
    for line in lines[1:]:
        columns = line.split()
        if len(columns) == 3 and all(c.isdigit() for c in columns):
            value = int(columns[1])
            if 0 < value < 9223372036854775807:
                present.add(value)
    times = sorted(present)
    gaps = [(b - a) / 1e6 for a, b in zip(times, times[1:])]
    return {'refresh_period_ns': period, 'present_samples': len(times),
            'window_gaps_gt25ms': sum(g > 25 for g in gaps),
            'window_max_gap_ms': max(gaps) if gaps else None}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb', default='adb')
    p.add_argument('--serial', required=True)
    p.add_argument('--out', type=Path, required=True, help='New private capture directory')
    p.add_argument('--mode', choices=['surface', 'texture', 'exo', 'yamby', 'observe'], required=True)
    p.add_argument('--clip', help='Existing device-local media path')
    p.add_argument('--codec', default='c2.rk.av1.decoder')
    p.add_argument('--frames', type=int)
    p.add_argument('--seconds', type=int, default=70)
    p.add_argument('--interval', type=float, default=5)
    p.add_argument('--configured-width', type=int, default=0, help='Diagnostic only: initial configured width for an info-change fallback test')
    p.add_argument('--configured-height', type=int, default=0)
    a = p.parse_args()
    diagnostic = a.mode in ('surface', 'texture')
    managed = diagnostic or a.mode == 'exo'
    package = 'com.rk3588.perftest' if diagnostic else 'com.rk3588.localtest'
    if a.seconds < 1 or a.interval <= 0:
        p.error('seconds and interval must be positive')
    if a.mode != 'observe' and not a.clip:
        p.error('clip is required when starting playback')
    if diagnostic and (not a.frames or a.frames < 1):
        p.error('frames must be an independently verified positive count')
    a.out.mkdir(parents=True, exist_ok=False)
    run_id = uuid.uuid4().hex
    label = 'path-' + run_id
    report_path = '/sdcard/Android/data/' + package + '/files/' + label + '.json'

    def adb(*args, timeout=25, check=True):
        result = subprocess.run([a.adb, '-s', a.serial, *args], capture_output=True,
                                encoding='utf-8', errors='replace', timeout=timeout)
        if check and result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout + result.stderr

    def shell(command, root=False):
        return adb('shell', 'su -c ' + shlex.quote(command) if root else command)

    meta = {'run_id': run_id, 'mode': a.mode, 'clip_name': Path(a.clip).name if a.clip else None,
            'codec': a.codec if diagnostic else None, 'expected_frames': a.frames,
            'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'samples': [], 'report_verified': False}
    (a.out / 'baseline.txt').write_text(shell(
        'getprop ro.board.platform; getprop ro.build.version.sdk; uname -r; '
        'getprop | grep rkcodec2; '
        'cat /data/adb/modules/rk3588_decode_stack/active*; '
        'cat /data/adb/modules/rk3588_mali_global/active-mode; '
        'cat /data/adb/modules/rk3588_mali_global/kernel/runtime-status.txt 2>/dev/null; true', True), encoding='utf-8')
    log_start = shell("date '+%m-%d %H:%M:%S.000'").strip()
    if managed:
        # Only stop this diagnostic; never kill unrelated applications.
        shell('am force-stop ' + package)
        command = ['am', 'start', '-n', package + '/.MainActivity',
                   '--es', 'clip', a.clip, '--es', 'codec', a.codec,
                   '--es', 'case', label, '--es', 'output', a.mode,
                   '--es', 'run_id', run_id, '--ei', 'frames', str(a.frames)]
        if diagnostic and a.configured_width > 0 and a.configured_height > 0:
            command += ['--ei', 'configured_width', str(a.configured_width),
                        '--ei', 'configured_height', str(a.configured_height)]
        if a.mode == 'exo':
            # Optional local diagnostic from the earlier device validation.
            # It loads the installed SmartTube engine; it is not bundled here.
            command = ['am', 'start', '-n', package + '/.MainActivity',
                       '--es', 'engine', 'exo', '--es', 'clip', a.clip,
                       '--es', 'case', label, '--ei', 'timeout_ms', str(a.seconds * 1000)]
        (a.out / 'launch.txt').write_text(shell(shlex.join(command)), encoding='utf-8')
    elif a.mode == 'yamby':
        command = ['am', 'start', '-a', 'android.intent.action.VIEW',
                   '-n', 'com.hush.yamby/.ui.activity.MpvPlayerProxyActivity',
                   '-d', 'file://' + a.clip, '-t', 'video/mp4']
        (a.out / 'launch.txt').write_text(shell(shlex.join(command)), encoding='utf-8')
    began = time.monotonic()
    i = 0
    try:
        while time.monotonic() - began < a.seconds:
            capture_start = time.monotonic() - began
            raw = shell(STATE, True)
            sf = shell('dumpsys SurfaceFlinger', True)
            (a.out / f'{i:03}-state.txt').write_text(raw, encoding='utf-8')
            (a.out / f'{i:03}-sf.txt').write_text(sf, encoding='utf-8')
            sample = parse_state(raw)
            sample.update(elapsed_seconds=capture_start,
                          capture_seconds=time.monotonic() - began - capture_start,
                          displays=parse_displays(sf))
            # Present-fence timestamps complement vendor callbacks, which may
            # simply echo the requested MediaCodec release time. Keep each
            # layer's ring buffer separate and do not reset shared statistics.
            if managed:
                layer = ('SurfaceView[com.rk3588.perftest/com.rk3588.perftest.MainActivity](BLAST)#0'
                         if a.mode == 'surface' else
                         'com.rk3588.perftest/com.rk3588.perftest.MainActivity#0' if diagnostic else
                         'SurfaceView[com.rk3588.localtest/com.rk3588.localtest.MainActivity](BLAST)#0')
                latency = shell('dumpsys SurfaceFlinger --latency ' + shlex.quote(layer), True)
                (a.out / f'{i:03}-latency.txt').write_text(latency, encoding='utf-8')
                sample['latency_window'] = parse_latency(latency)
            elif a.mode == 'yamby':
                names = shell('dumpsys SurfaceFlinger --list', True).splitlines()
                video_layers = [n for n in names if 'SurfaceView[' in n and 'com.hush.yamby/' in n]
                sample['latency_windows'] = {}
                for n, layer in enumerate(video_layers[:4]):
                    latency = shell('dumpsys SurfaceFlinger --latency ' + shlex.quote(layer), True)
                    (a.out / f'{i:03}-yamby-{n}-latency.txt').write_text(latency, encoding='utf-8')
                    sample['latency_windows'][layer] = parse_latency(latency)
            meta['samples'].append(sample)
            print(f'{i}: t={capture_start:.1f}s gpu={sample["gpu_load_pct"]}% '
                  f'freq={sample["gpu_freq_hz"]}', flush=True)
            i += 1
            if managed:
                report_raw = shell('if [ -f ' + shlex.quote(report_path) + ' ]; then cat ' +
                                   shlex.quote(report_path) + '; fi', True).strip()
                if report_raw:
                    report = json.loads(report_raw)
                    if (diagnostic and report.get('run_id') != run_id) or report.get('case') != label:
                        raise RuntimeError('Report identity mismatch')
                    meta['report_verified'] = True
                    meta['result'] = report
                    break
            remaining = a.seconds - (time.monotonic() - began)
            if remaining > 0:
                time.sleep(min(a.interval, remaining))
    finally:
        meta['elapsed_seconds'] = time.monotonic() - began
        (a.out / 'summary.json').write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding='utf-8')
        # Read logs since this run without clearing the shared log buffer.
        try:
            logs = shell('logcat -d -v threadtime -T ' + shlex.quote(log_start) +
                         ' -s RKPerf MPV mpv C2RKMpiDec C2RKMediaUtils CCodec Codec2Client RKPlaybackFix', True)
            (a.out / 'logcat.txt').write_text(logs, encoding='utf-8')
        except (RuntimeError, subprocess.TimeoutExpired) as e:
            (a.out / 'logcat-error.txt').write_text(str(e), encoding='utf-8')
        finally:
            if managed:
                try:
                    shell('am force-stop ' + package)
                except (RuntimeError, subprocess.TimeoutExpired) as e:
                    (a.out / 'cleanup-error.txt').write_text(str(e), encoding='utf-8')
    if managed and not meta['report_verified']:
        raise SystemExit('No fresh diagnostic report; capture is incomplete')
    print(json.dumps({k: v for k, v in meta.get('result', {'capture_complete': True}).items()
                      if k != 'samples'}, ensure_ascii=False, indent=2))
    if managed and (not meta['result'].get('complete') or
                    (diagnostic and not meta['result'].get('stop_ok'))):
        raise SystemExit('Diagnostic reported an incomplete or failed run')


if __name__ == '__main__':
    main()
