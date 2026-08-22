#!/usr/bin/env python3
"""navlab artifact 2 of 5: the timing-database generator.

Consumes artifact 1's normalized records and produces the versioned envelope
database the reachability navigator (artifact 3) will consult: for each
(locomotive, direction, route interval, PWM bucket), the measured dt
distribution of GENUINE single-interval traversals, with quantiles, sample
counts, and provenance.

Three fallback tiers, matching the plan's levels of knowledge:
  tier 1: (loco, dir, interval, pwm_bucket)          - confirmed position
  tier 2: (loco, dir, pwm_bucket) across intervals   - position uncertain
  tier 3: (loco, pwm_bucket)                         - long-term lost

Sample admission - stated, not implied:
  * label == 'genuine' ONLY. The 'phantom' label is known-contaminated: it
    trusts uncontradicted firmware rejections, and the firmware provably
    rejects runs of genuine markers. Phantom-labeled dts train nothing here.
  * consecutive same-boot records whose mm step is exactly +1 (CW) or -1
    (CCW): single-interval traversals, dt taken from the later record.
  * The MIN side of each envelope (fastest credible traversal) is the
    load-bearing bound for reachability; it is immune to dwell contamination.
    The MAX side is reported but a train can always be slower - the navigator
    must treat elapsed time, not p95, as the slow bound.

Hold-outs: --holdout SUBSTR excludes every session whose id contains SUBSTR
from envelope construction (plan artifact 4: entire untouched sessions).
Session id = <capture-basename>:<loco>:boot<N>. Excluded ids are listed in the
output header so a reviewer can verify the separation.

Usage:
  python3 tools/navlab/build_timing_db.py --records <records.jsonl>
      --out <timing_db.json> [--holdout SUBSTR ...] [--pwm-bucket 10]
"""
import argparse, datetime, json, sys
from collections import defaultdict

def q(sorted_v, p):
    if not sorted_v: return None
    i = min(len(sorted_v)-1, max(0, int(round(p*(len(sorted_v)-1)))))
    return sorted_v[i]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--records', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--holdout', action='append', default=[])
    ap.add_argument('--pwm-bucket', type=int, default=10)
    ap.add_argument('--margin', type=float, default=0.15,
                    help='safety margin applied to the credible bounds')
    args = ap.parse_args()

    rows = [json.loads(l) for l in open(args.records)]
    def sid(r): return f"{r['source']}:{r['loco']}:boot{r['boot']}"
    def all_ids(r): return [sid(r)] + list(r.get('dup_sources') or [])
    sessions = sorted({sid(r) for r in rows})
    held = sorted({s for s in sessions if any(h in s for h in args.holdout)})
    used = [s for s in sessions if s not in held]

    by_key = defaultdict(list)
    by_session = defaultdict(list)
    for r in rows: by_session[sid(r)].append(r)

    admitted = 0
    leaked = [0]
    for s, evs in by_session.items():
        if s in held: continue
        evs.sort(key=lambda r: r['ts'])
        for a, b in zip(evs, evs[1:]):
            step = (b['mm'] - a['mm']) % 171
            cw = (b.get('session_dir') or 'CW') == 'CW'
            if (step != 1 if cw else step != 170): continue
            # hold-out leak prevention: a physical event recorded by an
            # overlapping capture under a held-out identity trains nothing,
            # whichever copy survived dedupe
            if any(any(h in i for h in args.holdout)
                   for r2 in (a, b) for i in all_ids(r2)):
                leaked[0] += 1; continue
            if b['label'] != 'genuine' or a['label'] == 'phantom': continue
            if not b.get('dt_ms') or b['dt_ms'] <= 0: continue
            if b.get('pwm_actual') is None: continue
            pb = (b['pwm_actual'] // args.pwm_bucket) * args.pwm_bucket
            rec = (b['dt_ms'], s)
            by_key[('t1', b['loco'], b['session_dir'], b['interval'], pb)].append(rec)
            by_key[('t2', b['loco'], b['session_dir'], pb)].append(rec)
            by_key[('t3', b['loco'], pb)].append(rec)
            admitted += 1

    env = {}
    for k, v in by_key.items():
        dts = sorted(x[0] for x in v)
        m = args.margin
        env['|'.join(map(str, k))] = dict(
            n=len(dts), min=dts[0], p05=q(dts,.05), p25=q(dts,.25),
            p50=q(dts,.50), p95=q(dts,.95), max=dts[-1],
            # SAFETY MARGINS (plan artifact 2). fast_bound is the reachability
            # bound: no genuine traversal may be believed faster than this.
            # slow_soft is advisory only - slowness is bounded by elapsed
            # time, never by history.
            margin=m,
            fast_bound=int(dts[0] * (1 - m)),
            slow_soft=int(q(dts,.95) * (1 + m)),
            source_sessions=sorted({x[1] for x in v}))
    out = dict(
        version=1,
        generated=datetime.datetime.now().isoformat(timespec='seconds'),
        records_file=args.records, pwm_bucket=args.pwm_bucket,
        margin=args.margin,
        admission='genuine-labeled single-interval same-boot consecutive pairs',
        safety_note=('MIN side is the reachability bound; MAX side is '
                     'informational - slowness is bounded by elapsed time, '
                     'not by history'),
        sessions_used=used, sessions_held_out=held,
        holdout_leaks_blocked=leaked[0],
        build_command=' '.join(sys.argv),
        source_captures=sorted({r['source'] for r in rows}),
        admitted_samples=admitted, envelopes=env)
    json.dump(out, open(args.out, 'w'), indent=1)
    t1 = sum(1 for k in env if k.startswith('t1'))
    print(f'{admitted} samples -> {len(env)} envelopes ({t1} tier-1) '
          f'from {len(used)} sessions; {len(held)} held out -> {args.out}')

if __name__ == '__main__':
    sys.exit(main())
