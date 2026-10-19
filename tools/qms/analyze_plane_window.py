"""Correlate structural plane samples and DRM vblank events, never pixel content.

Inputs and output belong in private capture storage. A structurally unchanged
plane may contain black pixels, and IRQ timestamps are not wire measurements.
"""
import argparse
import json
from pathlib import Path
import re


def analyze(planes_text, vblank_text):
    records = [json.loads(line) for line in planes_text.splitlines() if line.startswith('{')]
    snapshots = [r for r in records if 'structure' in r]
    completed = next((r for r in reversed(records) if 'samples_completed' in r), None)
    if not snapshots or not completed:
        raise ValueError('Incomplete structural capture; do not infer an unchanged window')
    start = snapshots[0]['mono_start_ns']
    stop = start + completed['elapsed_ns']
    structural = []
    for row in snapshots:
        structure = row['structure']
        active = re.findall(r'^([^\n]+-win\d+): ACTIVE$', structure, re.M)
        structural.append({
            'relative_seconds': (row['mono_start_ns'] - start) / 1e9,
            'read_duration_ns': row['mono_end_ns'] - row['mono_start_ns'],
            'primary_active': 'Video Port0: ACTIVE\n' in structure,
            'active_planes': active,
            'formats_and_alpha': [s for s in structure.splitlines() if s.startswith('format:')],
        })
    # time= is the original kernel event timestamp, with the same monotonic
    # basis as the sampler; the timestamp printed by ftrace need not be equal.
    pattern = r'drm_vblank_event: crtc=0, seq=(\d+), time=(\d+), high-prec=(true|false)'
    events = [(int(seq), int(ns), high == 'true')
              for seq, ns, high in re.findall(pattern, vblank_text)]
    events = [e for e in events if start <= e[1] <= stop]
    counts = {'approximately_24Hz': 0, 'approximately_60Hz': 0, 'other': 0}
    previous = None
    missing = invalid = 0
    long = []
    segments = []
    for event in events:
        if previous is not None:
            delta = (event[0] - previous[0]) & 0xffffffff
            dt = event[1] - previous[1]
            if delta != 1:
                missing += 1
                previous = event
                continue  # Never reinterpret lost trace records as long frames.
            if dt <= 0:
                invalid += 1
                previous = event
                continue
            ms = dt / 1e6
            # Labels describe gross cadence, not clock-accuracy validation.
            label = ('approximately_24Hz' if abs(ms - 1000/24) < 2 else
                     'approximately_60Hz' if abs(ms - 1000/60) < 1.5 else 'other')
            counts[label] += 1
            if segments and segments[-1]['cadence'] == label:
                segments[-1]['end_seconds'] = (event[1] - start) / 1e9
                segments[-1]['intervals'] += 1
            else:
                segments.append({'cadence': label, 'start_seconds': (previous[1] - start) / 1e9,
                                 'end_seconds': (event[1] - start) / 1e9, 'intervals': 1})
            if dt > 100000000:
                long.append({'relative_seconds': (event[1] - start) / 1e9, 'interval_ms': ms})
        previous = event
    return {
        'diagnostic_only': True,
        'structural_capture': completed,
        'structural_changes_after_initial': len(snapshots) - 1,
        'structures': structural,
        'vblank_events_in_structural_window': len(events),
        'vblank_interval_counts': counts,
        'vblank_nonconsecutive_pairs': missing,
        'vblank_nonmonotonic_pairs': invalid,
        'vblank_timestamps_all_high_precision': bool(events) and all(e[2] for e in events),
        'vblank_intervals_over_100ms_with_consecutive_sequence': long,
        'cadence_segments_at_least_three_intervals': [s for s in segments if s['intervals'] >= 3],
        'limits': [
            'debugfs reports software plane state, not latched hardware enable or pixel values',
            '50ms structural sampling can miss shorter topology changes',
            'an active plane may contain black pixels or be covered by an opaque UI layer',
            'real DRM vblank events and IRQ timestamps do not prove transmitted VTEM integrity or receiver panel state',
            'no physical-blackout acceptance is inferred from this report',
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--planes', type=Path, required=True)
    parser.add_argument('--vblank', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = analyze(args.planes.read_text(encoding='utf-8'), args.vblank.read_text(encoding='utf-8'))
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k in (
        'structural_changes_after_initial', 'vblank_events_in_structural_window',
        'vblank_interval_counts', 'vblank_nonconsecutive_pairs',
        'vblank_intervals_over_100ms_with_consecutive_sequence')}))


if __name__ == '__main__':
    main()
