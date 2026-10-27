#!/usr/bin/env python3
"""EXPERIMENTAL map-aware matcher replay for NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md §4.5.

Replays the proposed single-template recognizer's events through two matchers:
  P8    the October 21 hold rule: advance to next only when polarity agrees, else hold.
  judge minimal map-aware matcher (experimental, NOT an approved navigation rule):
        each encounter is explained as next (+1), next after one miss (+2),
        next after two misses (+3) or not a magnet (0); a polarity that disagrees with
        the map costs C_MIS, each assumed miss C_SKIP, a non-magnet C_SP. NAVI reports
        the cheapest explanation of the history since the last anchor; a later
        encounter may revise it. lag 0 = decide on current evidence (the preferred
        form), lag 1/2 = wait that many encounters before reporting.
The penalties are hand-picked, untuned and unvalidated.

Scoring uses the research reference (route-anchored oracle, truth.py), not physical truth.
Each span starts from its first anchor; the anchor's own event is excluded (a + 0.05 s).
Run with NGR_NSR_NPZ and NGR_MF_CACHE set as in ../navi_matched_filter/README.md.
"""
import collections
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'navi_matched_filter'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import corpus          # noqa: E402
import mf              # noqa: E402
import truth           # noqa: E402
from design_analysis import PROPOSED  # noqa: E402

ROUTE = truth.route_polarity()
N_MM = 171
C_MIS, C_SKIP, C_SP = 1.0, 1.5, 1.5   # hand-picked, not tuned
PRUNE = 4.0                           # keep explanations within best + PRUNE
MOVES = ((1, 0.0), (2, C_SKIP), (3, 2 * C_SKIP), (0, C_SP))


def part_of(name):
    return 'oct27_toby' if '_20261027_' in name else corpus.partition_of(name)


def spans():
    names = sorted(set(corpus.sessions(['dev', 'validation', 'transfer'])))
    for n in names:
        s = corpus.load(n)
        ev, _ = mf.recognize(s['trace'], PROPOSED)
        corpus.score(s, ev)
        for a, b, d in s['spans']:
            f = [r for r in s['refs'] if r['kind'] == 'anchor' and abs(r['t'] - a) < 1e-6]
            if not f:
                continue
            es = [e for e in ev if a + 0.05 < e['t_peak'] <= b + 0.3]
            tm = [s['refs'][e['ref']]['mm'] if e['class'].startswith('magnet') and e['ref'] is not None
                  else None for e in es]
            yield part_of(n), f[0]['mm'], d, [e['pol'] for e in es], tm


def p8(start, d, pols):
    pos, lab = start, []
    for p in pols:
        t = (pos + d) % N_MM
        if p == ROUTE[t]:
            pos = t
            lab.append(t)
        else:
            lab.append('held')
    return lab, 0


def judge(start, d, pols, lag):
    """Returns per-event labels (mm, 'spurious' or 'held') and the number of revisions."""
    hist = [{start: (0.0, None)}]          # per encounter: mm -> (cost, (previous mm, move))
    lab, revisions, prev_best = [], 0, start
    for i, p in enumerate(pols):
        nxt = {}
        for pos, (c, _) in hist[-1].items():
            for k, ck in MOVES:
                q = (pos + d * k) % N_MM
                cc = c + ck + (0.0 if k == 0 or p == ROUTE[q] else C_MIS)
                if q not in nxt or cc < nxt[q][0]:
                    nxt[q] = (cc, (pos, k))
        best = min(v[0] for v in nxt.values())
        nxt = {q: v for q, v in nxt.items() if v[0] <= best + PRUNE}
        hist.append(nxt)
        q_best = min(nxt, key=lambda q: nxt[q][0])
        if nxt[q_best][1][0] != prev_best:  # the best history no longer passes through the last report
            revisions += 1
        prev_best = q_best
        j = i - lag
        if j >= 0:
            q = q_best
            for step in range(i, j, -1):
                q = hist[step + 1][q][1][0]
            lab.append('spurious' if hist[j + 1][q][1][1] == 0 else q)
    lab += ['held'] * (len(pols) - len(lab))
    return lab, revisions


def tally(lab, tm, c):
    run = 0
    for l, t in zip(lab, tm):
        if l == 'held':
            c['held_' + ('genuine' if t is not None else 'spurious')] += 1
            continue
        ok = (l == 'spurious') if t is None else (l == t)
        c['correct' if ok else 'wrong'] += 1
        if t is None:
            c['spurious_labelled_spurious'] += (l == 'spurious')
        if ok and run:
            c['wrong_episodes'] += 1
            c['longest_episode'] = max(c['longest_episode'], run)
            run = 0
        elif not ok:
            run += 1
    if run:
        c['wrong_episodes'] += 1
        c['longest_episode'] = max(c['longest_episode'], run)


def main():
    res = collections.defaultdict(collections.Counter)
    for part, st, d, pols, tm in spans():
        res[('P8', part)]['spurious_events'] += sum(t is None for t in tm)
        lab, _ = p8(st, d, pols)
        tally(lab, tm, res[('P8', part)])
        for lag in (0, 1, 2):
            lab, rev = judge(st, d, pols, lag)
            tally(lab, tm, res[('judge_lag%d' % lag, part)])
            res[('judge_lag%d' % lag, part)]['revisions'] += rev
    for k in sorted(res):
        print('%-11s %-11s %s' % (k[0], k[1], dict(sorted(res[k].items()))))


if __name__ == '__main__':
    main()
