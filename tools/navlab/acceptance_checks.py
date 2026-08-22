#!/usr/bin/env python3
"""navlab acceptance checker, iteration 3 CORRECTED semantics.

Three-state verdicts per condition: PASS / FAIL / NOT_DEMONSTRATED.
Overall PASS requires all nine PASS.

Iteration-3 CORRECTION round (operator directive, 2026-08-22). The first
iteration-3 checker still allowed conditions to pass on evidence that was only
internally consistent or that the replay never actually exercised:

  * C1 consumes the committed dt=0 probe (--probe). The corridor is only
    meaningful if it CONTAINS the true position; the probe demonstrates that
    under a prolonged dt-chain reset it does not, and that the navigator then
    confirms a position the locomotive did not occupy. A counterexample is a
    FAIL, whether or not the replayed data happens to contain one.
  * C5 counts only ghost events the navigator ACTUALLY PROCESSED. A replay
    that stops at event 85 of 4,726 cannot claim the session's later ghosts;
    the previous run credited 51 where 2 were seen.
  * C7 can never PASS on direction consistency. Direction monotonicity detects
    some false confirmations and proves no absolute position. C7 requires
    independently validated incident outcomes; without them it is
    NOT_DEMONSTRATED, and any detected false confirmation makes it FAIL.
  * C8 requires event-level justification whose basis is INDEPENDENT. Derived
    (not hand-pinned) window selection proves reproducible selection only.
    Amplitude/duration signatures are corpus heuristics and mm labels are
    firmware-derived, so with no operator fix inside any window C8 is
    NOT_DEMONSTRATED - see tools/navlab/classify_iter3_evidence.py.
  * Every condition carries an explicit evidence `basis`:
    independent | internal-consistency | development-regression | none.

Earlier iteration-3 corrections, retained:

  * Ground truth: a confirmation is VALIDATED only when independently
    anchored (operator declaration with no marker events between, or <=2
    events within 60 s). Direction monotonicity DETECTS some false
    confirmations but proves no absolute position - confirmations passing
    only that check are DIRECTION-CONSISTENT, and unvalidated ones are
    counted and reported. The permitted claim is "zero false confirmations
    DETECTED", never "zero false confirmations".
  * C5 cannot pass vacuously: it is NOT_DEMONSTRATED unless the runs
    exercised a non-empty, independently justified phantom population
    (ghost events: label 'phantom' AND peak < 90 in committed records).
  * C8: expectations must carry event-level justification. This checker
    DERIVES every exemplar window from committed records at runtime
    (firmware-rejected runs of >=5 within 8 s) - there are no hand-pinned
    expectations; C8 verifies that and fails if any manual window is
    supplied.
  * C9 is split: exclusion-from-training is hygiene (reported), but the
    condition PASSES only if every excluded-data replay ran to completion
    with every other condition passing on it.

Usage:
  python3 tools/navlab/acceptance_checks.py --records R --db D --capture C
      --report strict.json [--report ...] [--dev-report dev_strict.json ...]
      [--out results.json]
Reports given with --report are the evaluation set; --dev-report replays are
development data: they may demonstrate mechanisms (C5) but are labeled and
never presented as held-out performance.
"""
import argparse, json, pathlib, re, subprocess, sys

DNA_N = 171
P, F, ND = 'PASS', 'FAIL', 'NOT_DEMONSTRATED'

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

def derive_windows(rows):
    """Exemplar frozen-run windows DERIVED from committed records: runs of
    >=5 firmware-rejected events spaced <8 s. No hand-pinned epochs."""
    rej = sorted((r['ts'], r['mm']) for r in rows if r['fw_verdict'] == 'rejected')
    wins, cur = [], []
    for x in rej:
        if cur and x[0] - cur[-1][0] >= 8:
            if len(cur) >= 5: wins.append(cur)
            cur = []
        cur.append(x)
    if len(cur) >= 5: wins.append(cur)
    return [(c[0][0] - 0.5, c[-1][0] + 0.5,
             f'derived run of {len(c)} at mm{c[0][1]}') for c in wins]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--records', required=True)
    ap.add_argument('--db', required=True)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--report', action='append', default=[])
    ap.add_argument('--dev-report', action='append', default=[])
    ap.add_argument('--probe', help='committed dt=0 unknown-time probe result')
    ap.add_argument('--evidence', help='committed event-level expectation '
                                       'evidence (classify_iter3_evidence.py)')
    ap.add_argument('--out')
    args = ap.parse_args()
    dna, spc = load_map()
    db = json.load(open(args.db))
    allrec = [json.loads(l) for l in open(args.records)]

    NAMES = ['C1 never outside the physically reachable set',
             'C2 never behind the last confirmation without a reversal',
             'C3 no route-wide search before a full circuit is reachable',
             'C4 known genuine acceleration remains genuine',
             'C5 known phantoms do not advance confirmed position',
             'C6 frozen runs of genuine rejections disappear',
             'C7 known incidents recover or stop without false relocation',
             'C8 every expectation carries event-level justification',
             'C9 results hold on data excluded from envelope generation']
    BASIS = {1: 'internal-consistency', 2: 'internal-consistency',
             3: 'internal-consistency', 4: 'development-regression',
             5: 'development-regression', 6: 'development-regression',
             7: 'none', 8: 'none', 9: 'none'}
    C = {i: {'name': n, 'status': P, 'basis': BASIS[i], 'detail': []}
         for i, n in enumerate(NAMES, 1)}
    detected_false = []
    gt_stats = {'validated': 0, 'direction_consistent_only': 0, 'unvalidated': 0}
    ghosts_exercised = 0
    eval_complete = True
    incident_support = []

    # C1: the corridor is only meaningful if it CONTAINS the true position.
    # A committed counterexample refutes that, replay or no replay.
    if args.probe:
        pr = json.load(open(args.probe))
        bad = [r for r in pr['rows'] if r['confirmed_WRONG']]
        C[1]['detail'].append('replay check: claimed positions vs the '
                              "navigator's own corridor (internal consistency; "
                              'no independent position truth anywhere)')
        if bad:
            C[1]['status'] = F
            worst = max(bad, key=lambda r: r['confirmed_WRONG'])
            C[1]['detail'].append(
                'COUNTEREXAMPLE (%s): with %s interval(s) of travel during a '
                'dt-chain reset, %d of %d start positions CONFIRM a marker the '
                'locomotive does not occupy (error %s markers). The true '
                'position was outside the granted corridor, so the corridor is '
                'not a sound over-approximation.'
                % (pr['kind'], worst['true_intervals_during_unknown_time'],
                   worst['confirmed_WRONG'], worst['anchors_tried'],
                   worst['wrong_position_error_markers']))
            C[1]['detail'].append('no violation was detected in the replayed '
                                  'data; the counterexample is synthetic events '
                                  'on the real committed map')
    else:
        C[1]['status'] = ND
        C[1]['detail'].append('no containment probe supplied: the replay check '
                              'is internally consistent only')

    # C8: derived selection is reproducible, but the EXPECTATIONS still need
    # independent event-level justification.
    if args.evidence:
        evd = json.load(open(args.evidence))
        indep = [e for e in evd['c8_expectation_evidence']
                 if not e['evidence_classes']['independent_position_truth']
                 .upper().startswith('NONE')]
        C[8]['detail'].append(
            '%d expectations carry committed event-level evidence '
            '(tools/navlab/results/iter3_evidence_classification.json); '
            'independent position truth in %d of them'
            % (len(evd['c8_expectation_evidence']), len(indep)))
        for e in evd['c8_expectation_evidence']:
            C[8]['detail'].append(
                '%s: %s meet the genuine signature, %s polarity alternations '
                '(measurement); basis heuristic + firmware-derived; marker '
                'identity NOT established'
                % (e['window'], e['evidence']['events_meeting_genuine_signature'],
                   e['evidence']['polarity_alternations']))
        if not indep:
            C[8]['status'] = ND
            C[8]['detail'].append('NOT_DEMONSTRATED: no expectation is '
                                  'independently supported. Reproducible '
                                  'selection is not event-level justification.')
    else:
        C[8]['status'] = ND
        C[8]['detail'].append('no committed expectation evidence supplied')

    # Window SELECTION is derived, not hand-pinned - reproducible selection,
    # which is a different and weaker thing than justified expectations.
    C[8]['detail'].append('all exemplar windows derived from committed records '
                          'at runtime (firmware-rejected runs >=5, <8 s gaps); '
                          'no hand-pinned epochs - this establishes '
                          'REPRODUCIBLE SELECTION only')
    r = subprocess.run(['git', 'status', '--porcelain', 'firmware/'],
                       capture_output=True, text=True,
                       cwd=pathlib.Path(__file__).resolve().parents[2])
    if r.stdout.strip():
        C[8]['status'] = F
        C[8]['detail'].append('firmware/ tree modified: ' + r.stdout.strip()[:150])

    declares = []
    for line in open(args.capture, errors='replace'):
        p = line.split(' ', 2)
        if len(p) < 3 or not p[1].endswith('/state/nav'): continue
        try: d = json.loads(p[2])
        except ValueError: continue
        if isinstance(d, dict) and d.get('event') == 'DECLARED' and d.get('mm') is not None:
            declares.append((float(p[0]), p[1].split('/')[2], d['mm']))

    def walk(rp, is_dev):
        nonlocal ghosts_exercised, eval_complete
        rep = json.load(open(rp))
        loco = rep['loco']
        srcpat, boot = rep['session'].rsplit(':boot', 1)
        rows = sorted([r for r in allrec if r['loco'] == loco
                       and srcpat in r['source'] and r['boot'] == int(boot)],
                      key=lambda r: r['ts'])
        t0, t1 = rows[0]['ts'], rows[-1]['ts']
        log = rep['log']
        tag = ('DEV ' if is_dev else '') + f"{loco}:boot{boot}"

        if not is_dev:
            sid = f"{rows[0]['source']}:{loco}:boot{boot}"
            hyg = sid in db['sessions_held_out'] and sid not in db['sessions_used']
            C[9]['detail'].append(f'{tag}: excluded from training: {hyg}')
            if not hyg: C[9]['status'] = F
            if rep.get('final') != 'NORMAL':
                eval_complete = False
                C[9]['detail'].append(f'{tag}: replay did not run to completion '
                                      f'({rep.get("final")}) - excluded-data success not shown')
        if rep.get('externally_seeded', 0):
            detected_false.append((tag, 'externally seeded positions present'))

        step, anchor, hi = +1, None, 0.0
        lost = False; prev_conf = None
        confs = []
        for l in log:
            ev = l['ev']
            if ev == 'REVERSAL':
                step = +1 if l['to'] == 'CW' else -1
                anchor = (l['anchor'], l['t']); hi = l['hi']; prev_conf = None
            elif ev == 'DECLARED':
                anchor = (l['mm'], l['t']); hi = 0.0; lost = False
                # monotonicity comparisons must never span a declaration
                # boundary (a gated re-acquisition legitimately relocates)
                prev_conf = None
            elif ev == 'LOST_FULL_CIRCLE':
                lost = True
            elif ev == 'REACQUIRED':
                if not lost:
                    C[3]['status'] = F
                    C[3]['detail'].append(f'{tag}: REACQUIRED without LOST_FULL_CIRCLE at {l["t"]:.1f}')
            elif ev in ('STEP', 'CONFIRMED'):
                occ = l.get('P') or [l.get('mm')]
                la = l.get('anchor', anchor[0] if anchor else None)
                lhi = l.get('hi', hi)
                for pp in occ:
                    if la is None: continue
                    d = fwd_dist(spc, step, la, pp)
                    back = fwd_dist(spc, -step, la, pp)
                    if d > (lhi or 0) + 31:
                        if back <= 3 * 305:
                            C[2]['status'] = F
                            C[2]['detail'].append(f'{tag}: {pp} {back:.0f}mm BEHIND anchor {la} at {l["t"]:.1f}')
                        else:
                            C[1]['status'] = F
                            C[1]['detail'].append(f'{tag}: {pp} outside corridor ({d:.0f}mm > {lhi}mm) at {l["t"]:.1f}')
                if ev == 'CONFIRMED':
                    if prev_conf is not None:
                        fd = fwd_dist(spc, step, prev_conf, l['mm'])
                        if fd > sum(spc) * 0.6:
                            detected_false.append((tag, f'CONFIRMED {l["mm"]} at {l["t"]:.1f} against powered direction'))
                    prev_conf = l['mm']
                    confs.append(l)
                    anchor = (l['mm'], l['t']); hi = 0.0

        # ground-truth classes for this run's confirmations
        sess_dec = [(t, mm) for t, lo, mm in declares if lo == loco and t0 - 5 <= t <= t1 + 5]
        validated_ids = set()
        for dts, dmm in sess_dec:
            prior = [c for c in confs if c['t'] <= dts]
            if not prior: continue
            c = prior[-1]
            between = [r for r in rows if c['t'] < r['ts'] < dts]
            if len(between) == 0 or (dts - c['t'] <= 60 and len(between) <= 2):
                validated_ids.add(id(c))
                if wrapd(c['mm'], dmm) > 2:
                    detected_false.append((tag, f'CONFIRMED {c["mm"]} vs operator declare {dmm} at {dts:.1f}'))
        v = len(validated_ids)
        dc = len(confs) - v
        gt_stats['validated'] += v
        gt_stats['direction_consistent_only'] += dc
        C[7]['detail'].append(f'{tag}: confirmations {len(confs)}: {v} validated, '
                              f'{dc} direction-consistent only (NOT validated); '
                              f'{len(sess_dec)} operator declares in window')
        # C7 is about INCIDENT OUTCOMES: every route-wide re-acquisition and
        # every stop must be shown to have recovered to the right place, or
        # stopped without relocating. Direction consistency shows neither.
        inc = [l for l in log if l['ev'] in ('REACQUIRED', 'CONTRADICTION')]
        inc_val = 0
        for l in inc:
            for t, lo, _ in declares:
                if lo != loco or abs(t - l['t']) > 60:
                    continue
                # a declaration anchors an incident outcome only if no marker
                # event intervened; 50 events later it anchors nothing
                if not [r for r in rows if min(t, l['t']) < r['ts'] < max(t, l['t'])]:
                    inc_val += 1
                    break
        incident_support.append((tag, len(inc), inc_val))
        C[7]['detail'].append(
            f'{tag}: {len(inc)} incident outcomes (re-acquisitions and stops), '
            f'{inc_val} with independent support within 60 s')

        # exemplar windows, derived
        entries = {round(l['t'], 2): l for l in log if 't' in l}
        for w0, w1, name in derive_windows(rows):
            reached = any(l['t'] >= w0 for l in log)
            tgt = 4 if not is_dev else 4
            if not reached:
                if not is_dev:
                    C[4]['status'] = C[6]['status'] = F
                    C[4]['detail'].append(f'{tag}: {name}: NOT REACHED - fail, not partial')
                    C[6]['detail'].append(f'{tag}: {name}: NOT REACHED - fail, not partial')
                continue
            ge = [r for r in rows if w0 <= r['ts'] <= w1 and r['fw_verdict'] == 'rejected']
            adv = sum(1 for r in ge
                      if (entries.get(round(r['ts'], 2)) or {}).get('ev') in ('STEP', 'CONFIRMED'))
            ok = adv >= max(1, len(ge) - 1)
            if not ok and not is_dev:
                C[4]['status'] = C[6]['status'] = F
            C[4]['detail'].append(f'{tag}: {name}: {adv}/{len(ge)} firmware-rejected genuine events advanced')
            C[6]['detail'].append(f'{tag}: {name}: frozen run {"eliminated" if ok else "STILL PRESENT"}')

        # C5 ghosts: only counts as exercise if the population is non-empty
        # committed, independently justified phantom fixtures: the physical
        # ghost signature (peak < 90 AND duration < 80 ms; genuine markers
        # measure >=104 peak / >=116 ms - docs/QUORUM_LABEL_SLIP_ROOT_CAUSE.md)
        # plus any phantom-labeled sub-90 event.
        # ONLY events the navigator actually processed count. A replay that
        # stops early cannot claim the session's later ghosts.
        span_end = max((l['t'] for l in log if 't' in l), default=t0)
        ghosts_all = [r for r in rows
                      if ((r.get('peak') or 999) < 90 and (r.get('duration_ms') or 999) < 80)
                      or (r['label'] == 'phantom' and (r.get('peak') or 999) < 90)]
        ghosts = [r for r in ghosts_all if r['ts'] <= span_end]
        seen_ev = sum(1 for r in rows if r['ts'] <= span_end)
        if len(ghosts) != len(ghosts_all):
            C[5]['detail'].append(
                f'{tag}: replay processed {seen_ev}/{len(rows)} events; '
                f'{len(ghosts_all) - len(ghosts)} of {len(ghosts_all)} session '
                f'ghosts were never seen and are NOT credited')
        prevP = None; advanced = 0
        for l in log:
            if l['ev'] in ('STEP', 'CONFIRMED'):
                curP = set(l.get('P') or [l.get('mm')])
                for g in ghosts:
                    if abs(l['t'] - g['ts']) < 0.05 and prevP is not None and curP != prevP:
                        advanced += 1
                prevP = curP
        if ghosts:
            ghosts_exercised += len(ghosts)
            if advanced:
                C[5]['status'] = F
                C[5]['detail'].append(f'{tag}: {advanced}/{len(ghosts)} ghosts ADVANCED position')
            else:
                C[5]['detail'].append(f'{tag}: {len(ghosts)} ghosts exercised, none advanced')
        else:
            C[5]['detail'].append(f'{tag}: zero ghosts in this session - contributes nothing to C5')

    for rp in args.report: walk(rp, False)
    for rp in args.dev_report: walk(rp, True)
    if not args.report:
        C[9]['status'] = ND
        C[9]['detail'].append('NO genuinely untouched evaluation data exists: every '
                              'committed session was used for envelope training, was a '
                              'development session, or has been event-level inspected in '
                              'prior iterations. Dev replays below demonstrate mechanisms '
                              'only. Unbiased validation requires future data.')

    if ghosts_exercised == 0 and C[5]['status'] == P:
        C[5]['status'] = ND
        C[5]['detail'].append('no phantom population exercised anywhere - a vacuous zero cannot pass')
    # A handful of processed ghosts is an anecdote, not a demonstration.
    MIN_GHOSTS = 20
    if C[5]['status'] == P and ghosts_exercised < MIN_GHOSTS:
        C[5]['status'] = ND
        C[5]['detail'].append(f'only {ghosts_exercised} ghost events were '
                              f'actually processed (< {MIN_GHOSTS}) - too thin '
                              f'to demonstrate phantom immunity')

    # C4/C6 expectations rest on unvalidated positions unless C8 stands.
    for i in (4, 6):
        if C[i]['status'] == P and C[8]['status'] != P:
            C[i]['status'] = ND
            C[i]['detail'].append('expectation basis is heuristic/'
                                  'firmware-derived (C8 NOT_DEMONSTRATED): the '
                                  'runs did advance, but "correctly" is not '
                                  'established by independent evidence')

    # C7: direction consistency can never carry it.
    tot_inc = sum(n for _, n, _ in incident_support)
    val_inc = sum(v for _, _, v in incident_support)
    if detected_false:
        C[7]['status'] = F
    elif gt_stats['validated'] == 0 or val_inc < tot_inc or tot_inc == 0:
        C[7]['status'] = ND
        C[7]['detail'].append(
            f'NOT_DEMONSTRATED: {val_inc} of {tot_inc} incident outcomes have '
            f'independent support, and {gt_stats["validated"]} of '
            f'{gt_stats["validated"] + gt_stats["direction_consistent_only"]} '
            f'confirmations are independently validated. Direction consistency '
            f'detects contradictions; it proves no absolute position.')
    else:
        C[7]['basis'] = 'independent'
    if C[9]['status'] != F:
        others_ok = all(C[i]['status'] == P for i in (1, 2, 3, 4, 5, 6, 7, 8))
        if not (eval_complete and others_ok):
            C[9]['status'] = ND if C[9]['status'] == P else C[9]['status']
            C[9]['detail'].append('exclusion hygiene alone does not show results HELD on excluded '
                                  'data: requires completed replays with all other conditions passing')

    n_pass = sum(1 for c in C.values() if c['status'] == P)
    n_fail = sum(1 for c in C.values() if c['status'] == F)
    overall = P if n_pass == 9 and not detected_false else F
    claim = ('zero false confirmations DETECTED (direction monotonicity + '
             f'{gt_stats["validated"]} independently validated); '
             f'{gt_stats["direction_consistent_only"]} confirmations remain unvalidated')
    print(f'== acceptance: {overall} | PASS {n_pass}, FAIL {n_fail}, '
          f'NOT_DEMONSTRATED {sum(1 for c in C.values() if c["status"]==ND)} ==')
    print('   ground truth:', claim)
    for i, c in sorted(C.items()):
        print(f'  {c["status"]:<16} [{c["basis"]:<23}] {c["name"]}')
        for d in c['detail'][:5]: print('        ' + d)
    for t, m in detected_false[:8]: print('  DETECTED-FALSE: ' + t + ': ' + m)
    if args.out:
        json.dump(dict(overall=overall, ground_truth_claim=claim,
                       gt_stats=gt_stats,
                       ghost_events_actually_processed=ghosts_exercised,
                       incident_outcomes=dict(
                           total=sum(n for _, n, _ in incident_support),
                           independently_supported=sum(v for _, _, v in incident_support),
                           per_run=[dict(run=t, incidents=n, supported=v)
                                    for t, n, v in incident_support]),
                       detected_false=[f'{a}: {b}' for a, b in detected_false],
                       conditions={f'C{i}': c for i, c in C.items()}),
                  open(args.out, 'w'), indent=1)
        print('->', args.out)
    return 0 if overall == P else 1

if __name__ == '__main__':
    sys.exit(main())
