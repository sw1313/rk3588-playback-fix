"""Measure a bounded GPU burst workload under the already active module policy.

Private CSV/state captures stay in the explicitly supplied output directory.
This tool never writes devfreq or changes the selected module policy.
"""
import argparse
import csv
import json
from pathlib import Path
import shlex
import statistics
import subprocess


def summarize(rows):
    groups = {}
    for row in rows:
        groups.setdefault(int(row['burst']), []).append(row)
    bursts = []
    for index, group in sorted(groups.items()):
        first, last = group[0], group[-1]
        bursts.append({
            'burst': index,
            'frames': len(group),
            'start_frequency_hz': int(first['freq_before']),
            'draw_finish_sum_ms': sum(int(r['elapsed_ns']) for r in group) / 1e6,
            'wall_ms': (int(last['start_ns']) + int(last['elapsed_ns']) - int(first['start_ns'])) / 1e6,
            'first_frame_ms': int(first['elapsed_ns']) / 1e6,
        })
    if not bursts:
        raise ValueError('No measured bursts')
    hashes = sorted(set(int(r['pixel_hash']) for r in rows))
    if len(hashes) != 1:
        raise ValueError('GPU pixel readback changed across the workload')
    return {
        'bursts': bursts,
        'total_frames': len(rows),
        'draw_finish_sum_p50_ms': statistics.median(b['draw_finish_sum_ms'] for b in bursts),
        'burst_wall_p50_ms': statistics.median(b['wall_ms'] for b in bursts),
        'first_frame_p50_ms': statistics.median(b['first_frame_ms'] for b in bursts),
        'pixel_hash': hashes[0],
        'scope': 'Offscreen synthetic GLES bursts; no game FPS or display pacing claim',
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--adb', default='adb')
    p.add_argument('--serial', required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--bursts', type=int, default=12, choices=range(1, 41))
    p.add_argument('--frames', type=int, default=12, choices=range(1, 121))
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    adb = [a.adb, '-s', a.serial]

    def remote(command, timeout=15):
        return subprocess.run(adb + ['shell', 'su -c ' + shlex.quote(command)],
                              capture_output=True, text=True, encoding='utf-8',
                              errors='replace', timeout=timeout, check=True)

    state_command = ('sh /data/adb/modules/rk3588_mali_global/webui.sh status; '
                     'for f in governor polling_interval min_freq max_freq available_frequencies; do '
                     'echo "$f"; cat /sys/class/devfreq/fb000000.gpu/"$f"; done; '
                     'for t in /sys/class/thermal/thermal_zone*; do cat "$t/type" "$t/temp"; done; '
                     'sha256sum /data/adb/modules/rk3588_decode_stack/components/codec2_sdk_next/system/vendor/lib/hw/libcodec2_rx_component.so')
    (a.out / 'before.txt').write_text(remote(state_command).stdout, encoding='utf-8')
    ready = remote('getprop vendor.rkmaliglobal.ready; getprop vendor.rkmalikernel.ready').stdout.splitlines()
    if ready != ['active', 'active']:
        raise RuntimeError('Wait for both userspace and paired kernel qualification')
    device_binary = '/data/local/tmp/rk-dvfs-burst64'
    subprocess.run(adb + ['push', str(a.binary), device_binary], check=True, capture_output=True)
    remote('chmod 0755 ' + device_binary)
    result = remote('LD_LIBRARY_PATH=/vendor/lib64:/system/lib64:/apex/com.android.vndk.v32/lib64 '
                    + device_binary + f' {a.bursts} {a.frames}', timeout=max(45, a.bursts * a.frames * 0.1 + a.bursts + 20))
    (a.out / 'frames.csv').write_text(result.stdout, encoding='utf-8')
    (a.out / 'driver.txt').write_text(result.stderr, encoding='utf-8')
    rows = list(csv.DictReader(result.stdout.splitlines()))
    if len(rows) != a.bursts * a.frames:
        raise RuntimeError('Incomplete burst/frame delivery')
    summary = summarize(rows)
    (a.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    (a.out / 'after.txt').write_text(remote(state_command).stdout, encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k != 'bursts'}, indent=2))


if __name__ == '__main__':
    main()
