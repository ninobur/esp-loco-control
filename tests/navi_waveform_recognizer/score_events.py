#!/usr/bin/env python3
"""Score replay-harness events against a reference, or report them.

usage:
  score_events.py events.csv <ticks.json> [--ref level1_ref.json]
  score_events.py events.csv <ticks.json> --report   (no reference: list only)

The reference JSON is a list of {t_peak_s, polarity, ref_mm} (times relative
to the recording start t0_us) plus a scored span [span_start_s, span_end_s].
Events inside the span are matched to the nearest reference magnet within
+/-0.15 s; a match with the wrong polarity counts as a polarity error (the
magnet is still counted as found). Events outside the span are unscored.
"""
import csv
import json
import sys
from pathlib import Path

MATCH_TOL_S = 0.15


def load_events(path):
    evs = []
    with open(path) as f:
        for r in csv.DictReader(f):
            evs.append({'passage_us': int(r['passage_us']),
                        'recognized_us': int(r['recognized_us']),
                        'r': float(r['score_r']),
                        'a': float(r['amplitude_adc']),
                        'pol': int(r['polarity'])})
    return evs


def main():
    events_csv, ticks_json = sys.argv[1], sys.argv[2]
    report = '--report' in sys.argv
    ref_path = None
    if '--ref' in sys.argv:
        ref_path = sys.argv[sys.argv.index('--ref') + 1]
    meta = json.loads(Path(ticks_json).read_text())
    evs = load_events(events_csv)
    if 'panels' in meta:
        # Multi-panel input (CSV): each event is reported relative to the
        # panel it falls in.
        panels = meta['panels']
        t0 = panels[0]['t0_us']
        for e in evs:
            p = next((q for q in panels
                      if q['t0_us'] <= e['passage_us'] <=
                      q['t0_us'] + int(q['duration_s'] * 1e6)), None)
            e['t_s'] = ((e['passage_us'] - p['t0_us']) / 1e6) if p else -1
            e['panel'] = panels.index(p) + 1 if p else 0
    else:
        t0 = meta['t0_us']
        for e in evs:
            e['t_s'] = (e['passage_us'] - t0) / 1e6
            e['panel'] = 0
    for e in evs:
        e['latency_ms'] = (e['recognized_us'] - e['passage_us']) / 1000.0
    print(f"events: {len(evs)}")
    if evs:
        print(f"latency ms: min {min(e['latency_ms'] for e in evs):.0f} "
              f"max {max(e['latency_ms'] for e in evs):.0f} "
              f"(design: 359)")
    if report or ref_path is None:
        for e in evs:
            panel = f" panel {e['panel']}" if e.get('panel') else ""
            print(f"  t={e['t_s']:8.3f}s{panel} pol={e['pol']:+d} "
                  f"r={e['r']:+.3f} A={e['a']:+7.1f}")
        return
    ref = json.loads(Path(ref_path).read_text())
    refs = ref['magnets']
    span = ref['span_s']
    # Match every event to its nearest reference within tolerance first;
    # the span only decides whether an unmatched event is false (inside)
    # or unscored (outside). A magnet at the span's edge can legitimately
    # be recognized a few ms past the edge (the passage time is the peak
    # + 2 ms), so matching must not be cut off by the span.
    used = set()
    found = pol_err = 0
    false_ev = []
    pairs = []  # (reference, event) matched pairs, for timing errors
    for e in evs:
        best, best_dt = None, 1e9
        for i, m in enumerate(refs):
            if i in used:
                continue
            dt = abs(e['t_s'] - m['t_peak_s'])
            if dt < best_dt:
                best, best_dt = i, dt
        if best is not None and best_dt <= MATCH_TOL_S:
            used.add(best)
            pairs.append((refs[best], e))
            found += 1
            if e['pol'] != refs[best]['polarity']:
                pol_err += 1
                print(f"  POLARITY ERROR t={e['t_s']:.3f}s "
                      f"ref MM{refs[best]['ref_mm']} "
                      f"{refs[best]['polarity']:+d} observed {e['pol']:+d}")
        elif span[0] <= e['t_s'] <= span[1]:
            false_ev.append(e)
    missed = [m for i, m in enumerate(refs) if i not in used]
    print(f"reference magnets in span: {len(refs)}")
    print(f"found: {found}  missed: {len(missed)}  polarity errors: {pol_err} "
          f"false events in span: {len(false_ev)}")
    for m in missed:
        print(f"  MISSED t={m['t_peak_s']:.3f}s MM{m['ref_mm']} "
              f"pol={m['polarity']:+d}")
    for e in false_ev:
        print(f"  FALSE t={e['t_s']:.3f}s pol={e['pol']:+d} "
              f"r={e['r']:+.3f} A={e['a']:+.1f}")
    timing = [(e['t_s'] - m['t_peak_s']) * 1000 for m, e in pairs]
    if timing:
        print(f"passage timing error ms: min {min(timing):+.1f} "
              f"max {max(timing):+.1f} mean {sum(timing)/len(timing):+.1f}")


if __name__ == '__main__':
    main()
