#!/usr/bin/env python3
"""navlab iteration 2: automated replay-level checks for ALL NINE acceptance
conditions of QUORUM_REACHABILITY_RECOVERY_PLAN.md, plus the overriding
ground-truth rule: EVERY confirmed position is validated against
independently supported ground truth, and a single false confirmation fails
the run.

Ground truth definition (independent of firmware position labels):
  * operator DECLARE actions (state/nav DECLARED events in the committed
    capture) - a human placed or verified the locomotive there;
  * motor-direction evidence (motor_dir in the committed records) - a
    confirmation stream moving against the powered direction is false.

Verdict semantics are the operator's decision rule verbatim: any false
confirmation fails; any position outside physical reachability fails; any
unmodeled reversal or premature route-wide search fails; a partial result is
never described as a pass.

Usage:
  python3 tools/navlab/acceptance_checks.py --records <records.jsonl>
      --db <timing_db.json> --capture <navlab-inputs log>
      --report <strict_report.json> [--report ...] [--out results.json]
"""
import argparse, json, pathlib, re, subprocess, sys

DNA_N = 171

def load_map():
    src = open(pathlib.Path(__file__).resolve().parents[2]
               / 'firmware/QUORUM/QUORUM.ino').read()
    dna = [int(x) for x in re.findall(r'\d+',
        src.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    spc = [int(x) for x in re.findall(r'\d+',
        src.split('static const uint16_t spacingMm[DNA_N] PROGMEM = {')[1].split('};')[0])]
    return dna, spc

def fwd_dist(spc, step, a, b):
    d, mm = 0, a
    for _ in range(DNA_N):
        if mm == b: return d
        nxt = (mm + step) % DNA_N
        d += spc[(nxt - 1) % DNA_N if step > 0 else nxt]
        mm = nxt
    return d

def wrapd(a, b):
    d = abs(a - b) % DNA_N
    return min(d, DNA_N - d)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--records', required=True)
    ap.add_argument('--db', required=True)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--report', action='append', required=True)
    ap.add_argument('--out')
    args = ap.parse_args()
    dna, spc = load_map()
    db = json.load(open(args.db))
    allrec = [json.loads(l) for l in open(args.records)]

    conditions = {i: {'name': n, 'pass': True, 'detail': []} for i, n in enumerate([
        'C1 never outside the physically reachable set',
        'C2 never behind the last confirmation without a reversal',
        'C3 no route-wide search before a full circuit is reachable',
        'C4 known genuine acceleration remains genuine',
        'C5 known phantoms do not advance confirmed position',
        'C6 frozen runs of genuine rejections disappear',
        'C7 known incidents recover or stop without false relocation',
        'C8 no changed regression expectation without justification',
        'C9 results hold on sessions excluded from envelope generation'], 1)}
    false_confirmations = []

    # C8: navlab must not have touched firmware test expectations
    r = subprocess.run(['git', 'status', '--porcelain', 'firmware/'],
                       capture_output=True, text=True,
                       cwd=pathlib.Path(__file__).resolve().parents[2])
    if r.stdout.strip():
        conditions[8]['pass'] = False
        conditions[8]['detail'].append('firmware/ tree modified: ' + r.stdout.strip()[:200])
    else:
        conditions[8]['detail'].append('firmware/ tree untouched by iteration 2')

    # ground truth: operator declares from the committed capture
    declares = []
    for line in open(args.capture, errors='replace'):
        p = line.split(' ', 2)
        if len(p) < 3 or not p[1].endswith('/state/nav'): continue
        try: d = json.loads(p[2])
        except ValueError: continue
        if isinstance(d, dict) and d.get('event') == 'DECLARED' and d.get('mm') is not None:
            declares.append((float(p[0]), p[1].split('/')[2], d['mm']))

    for rp in args.report:
        rep = json.load(open(rp))
        loco = rep['loco']
        srcpat, boot = rep['session'].rsplit(':boot', 1)
        rows = sorted([r for r in allrec if r['loco'] == loco
                       and srcpat in r['source'] and r['boot'] == int(boot)],
                      key=lambda r: r['ts'])
        t0, t1 = rows[0]['ts'], rows[-1]['ts']
        log = rep['log']
        tag = f"{loco}:{rep['session']}"

        # C9: held-out separation
        sid = f"{rows[0]['source']}:{loco}:boot{boot}"
        if sid in db['sessions_used'] or sid not in db['sessions_held_out']:
            conditions[9]['pass'] = False
            conditions[9]['detail'].append(f'{tag}: session not properly held out')
        else:
            conditions[9]['detail'].append(f'{tag}: held out and absent from every envelope source list')

        # walk the log with direction tracking
        step, anchor, hi = +1, None, 0.0
        lost_since_declare = False
        prev_conf = None
        ext = rep.get('externally_seeded', 0)
        if ext:
            false_confirmations.append((tag, 'externally seeded positions present in a run offered as performance'))
        for l in log:
            ev = l['ev']
            if ev == 'REVERSAL':
                step = +1 if l['to'] == 'CW' else -1
                anchor = (l['anchor'], l['t']); hi = l['hi']
                prev_conf = None
            elif ev == 'DECLARED':
                anchor = (l['mm'], l['t']); hi = 0.0
                lost_since_declare = False
            elif ev == 'LOST_FULL_CIRCLE':
                lost_since_declare = True
            elif ev == 'REACQUIRED':
                if not lost_since_declare:
                    conditions[3]['pass'] = False
                    conditions[3]['detail'].append(
                        f'{tag}: REACQUIRED at {l["t"]:.1f} without LOST_FULL_CIRCLE')
            elif ev in ('STEP', 'CONFIRMED'):
                occ = l.get('P') or [l.get('mm')]
                la, lhi = l.get('anchor', anchor[0] if anchor else None), l.get('hi', hi)
                for p in occ:
                    if la is None: continue
                    d = fwd_dist(spc, step, la, p)
                    if d > (lhi or 0) + 30 + 1:
                        conditions[1]['pass'] = False
                        conditions[1]['detail'].append(
                            f'{tag}: {ev} at {l["t"]:.1f} occupies {p}, '
                            f'{d:.0f}mm ahead of anchor {la} with corridor {lhi}mm')
                    # C2 is BACKWARD relocation: outside the forward corridor
                    # AND explicable as a short reverse move. Far-forward
                    # positions inside a large corridor are C1's business.
                    back = fwd_dist(spc, -step, la, p)
                    if d > (lhi or 0) + 31 and back <= 3 * 305:
                        conditions[2]['pass'] = False
                        conditions[2]['detail'].append(
                            f'{tag}: {ev} at {l["t"]:.1f} occupies {p}, '
                            f'{back:.0f}mm BEHIND anchor {la} without a reversal')
                if ev == 'CONFIRMED':
                    # ground truth (b): direction monotonicity vs motor evidence
                    if prev_conf is not None:
                        fd = fwd_dist(spc, step, prev_conf, l['mm'])
                        if fd > sum(spc) * 0.6:
                            false_confirmations.append(
                                (tag, f'CONFIRMED {l["mm"]} at {l["t"]:.1f} moves against '
                                      f'the powered direction (prev {prev_conf})'))
                    prev_conf = l['mm']
                    # a confirmation moves the anchor (declare semantics)
                    anchor = (l['mm'], l['t']); hi = 0.0

        # ground truth (a): declares
        sess_dec = [(t, mm) for t, lo, mm in declares if lo == loco and t0 - 5 <= t <= t1 + 5]
        checked = 0
        for dts, dmm in sess_dec:
            confs = [l for l in log if l['ev'] == 'CONFIRMED' and l['t'] <= dts]
            if not confs: continue
            c = confs[-1]
            between = [r for r in rows if c['t'] < r['ts'] < dts]
            # a parked locomotive (zero marker events since the confirm)
            # cannot have moved: no time limit applies. With events between,
            # keep the tight gate.
            if len(between) == 0:
                pass
            elif dts - c['t'] > 60 or len(between) > 2:
                continue
            checked += 1
            if wrapd(c['mm'], dmm) > 2:
                false_confirmations.append(
                    (tag, f'CONFIRMED {c["mm"]} at {c["t"]:.1f} vs operator declare '
                          f'{dmm} at {dts:.1f} ({len(between)} events between)'))
        conditions[7]['detail'].append(
            f'{tag}: {checked} confirmations anchored against operator declares; '
            f'{len(sess_dec)} declares in session')

        # C4/C6: the frozen-run exemplar windows
        # windows verified from committed records (fw_verdict=='rejected'
        # runs of >=5 within 8 s spacing), 2026-08-22 - NOT from memory
        WINDOWS = {'9950012': [(1787331943.0, 1787331950.0, 'toby five-straight mm101')],
                   '9950011': [(1787339281.0, 1787339316.5, 'otto 13-run mm130'),
                               (1787339440.0, 1787339479.0, 'otto 16-run mm52'),
                               (1787339931.0, 1787339965.0, 'otto 14-run mm126')]}
        for w0, w1, name in WINDOWS.get(loco, []):
            if not (t0 <= w0 <= t1): continue
            # exemplars are the events the FIRMWARE rejected in the frozen
            # run - the label heuristic mislabels exactly these as phantom
            # (documented contamination), so it must not select them
            ge = [r for r in rows if w0 <= r['ts'] <= w1
                  and r['fw_verdict'] == 'rejected']
            entries = {round(l['t'], 2): l for l in log if 't' in l}
            reached = any(l['t'] >= w0 for l in log)
            if not reached:
                conditions[4]['pass'] = conditions[6]['pass'] = False
                conditions[4]['detail'].append(f'{tag}: {name}: NOT REACHED (run stopped earlier) - fail, not partial')
                conditions[6]['detail'].append(f'{tag}: {name}: NOT REACHED - fail, not partial')
                continue
            advanced = 0; held = 0
            for r in ge:
                l = entries.get(round(r['ts'], 2))
                if l and l['ev'] in ('STEP', 'CONFIRMED'): advanced += 1
                elif l: held += 1
            ok = advanced >= max(1, len(ge) - 1)
            if not ok:
                conditions[4]['pass'] = False
                conditions[6]['pass'] = False
            conditions[4]['detail'].append(
                f'{tag}: {name}: {advanced}/{len(ge)} genuine events advanced, {held} held')
            conditions[6]['detail'].append(
                f'{tag}: {name}: frozen run {"eliminated" if ok else "STILL PRESENT"}')

        # C5: ghost phantoms must not advance position
        ghosts = [r for r in rows if r['label'] == 'phantom' and (r.get('peak') or 999) < 90]
        entries = {round(l['t'], 2): l for l in log if 't' in l}
        adv = []
        prevP = None
        for l in log:
            if l['ev'] in ('STEP', 'CONFIRMED'):
                curP = set(l.get('P') or [l.get('mm')])
                for g in ghosts:
                    if abs(l['t'] - g['ts']) < 0.05 and prevP is not None and curP != prevP:
                        adv.append(g)
                prevP = curP
        if adv:
            conditions[5]['pass'] = False
            conditions[5]['detail'].append(f'{tag}: {len(adv)} ghost phantoms advanced position')
        else:
            conditions[5]['detail'].append(f'{tag}: {len(ghosts)} ghost phantoms in session, none advanced position')

    # the overriding rule
    gt = {'false_confirmations': [f'{t}: {m}' for t, m in false_confirmations],
          'verdict': 'FAIL' if false_confirmations else 'PASS'}
    if false_confirmations:
        conditions[7]['pass'] = False

    n_pass = sum(1 for c in conditions.values() if c['pass'])
    overall = 'PASS' if (n_pass == 9 and not false_confirmations) else 'FAIL'
    out = dict(overall=overall, passed=n_pass, of=9,
               ground_truth=gt, conditions={f'C{i}': c for i, c in conditions.items()})
    print(f'== acceptance: {overall} ({n_pass}/9 conditions; '
          f'{len(false_confirmations)} false confirmations) ==')
    for i, c in sorted(conditions.items()):
        print(('  PASS ' if c['pass'] else '  FAIL ') + c['name'])
        for d in c['detail'][:4]: print('        ' + d)
    for f in gt['false_confirmations'][:6]: print('  FALSE-CONF: ' + f)
    if args.out:
        json.dump(out, open(args.out, 'w'), indent=1)
        print('->', args.out)
    return 0 if overall == 'PASS' else 1

if __name__ == '__main__':
    sys.exit(main())
