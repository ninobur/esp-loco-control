#!/usr/bin/env python3
"""navlab artifact 3 of 5: the forward-reachability navigator (host harness).

Replaces fixed-offset guessing with physical reachability, per
QUORUM_REACHABILITY_RECOVERY_PLAN.md:

  anchor      last confirmed position + time (+ direction);
  corridor    [lo, hi] millimetres the locomotive could have travelled since
              the anchor, integrated from the recorded PWM history against
              the measured timing envelopes (artifact 2). The fast side uses
              each PWM bucket's fast_bound (measured minimum dt with safety
              margin); the slow side is zero - slowness is bounded by elapsed
              time, never by history;
  hypotheses  the set P of positions the locomotive may occupy NOW. Every
              member is ahead of the anchor and inside the corridor. P moves
              only forward; it may never contain a position behind the last
              confirmation while travel direction is unchanged;
  pending     a doubtful event is HELD, never discarded: the successor event
              is evaluated under both hypotheses (pending genuine / pending
              phantom) and the union survives;
  contradiction  if no hypothesis survives and a full circuit is not yet
              physically reachable, the navigator STOPS and reports. It never
              searches the whole railway to escape a contradiction.

DNA (magnet polarity) eliminates candidates INSIDE the corridor only. It can
never propose a position physics excludes.

This is a host tool. No firmware flag, no MQTT, no Pi contract.

Usage:
  python3 tools/navlab/reachability_nav.py --records <records.jsonl>
      --db <timing_db.json> --loco 9950012 --session <source:boot>
      [--report out.json]
"""
import argparse, json, sys
from collections import namedtuple

DNA_N = 171

class Envelopes:
    def __init__(self, db):
        self.e = db['envelopes']
        self.margin = db.get('margin', 0.15)
        # per-loco fastest credible speed (mm/ms) across all buckets, the
        # conservative ceiling when a PWM bucket was never measured
        self.vmax_global = {}
        for k, v in self.e.items():
            p = k.split('|')
            if p[0] == 't3':
                loco = p[1]
                vm = 305.0 / max(1, v['fast_bound'])
                self.vmax_global[loco] = max(self.vmax_global.get(loco, 0), vm)

    def fast_dt(self, loco, sdir, interval, pwm_bucket, spacing):
        """Fastest credible ms for ONE interval at this pwm (tiered)."""
        for key in (f't1|{loco}|{sdir}|{interval}|{pwm_bucket}',
                    f't2|{loco}|{sdir}|{pwm_bucket}',
                    f't3|{loco}|{pwm_bucket}'):
            v = self.e.get(key)
            if v and v['n'] >= 5:
                return v['fast_bound']
        vm = self.vmax_global.get(loco)
        return spacing / vm if vm else 0     # 0 = no constraint known

    def vmax(self, loco, pwm_bucket):
        # Dead zone: below MOTOR_DEAD_ZONE_PWM the locomotive cannot
        # self-propel. 0.15 mm/ms is a generous coast/push ceiling; the
        # corridor loop zeroes it once the zero-stretch persists.
        if pwm_bucket <= 20:
            return 0.15
        v = self.e.get(f't3|{loco}|{pwm_bucket}')
        if v and v['n'] >= 5:
            return 305.0 / max(1, v['fast_bound'])       # mm per ms
        # Unmeasured driving bucket: inherit the nearest MEASURED bucket
        # ABOVE (faster = conservative, but never the global maximum for a
        # crawl duty).
        for b in range(pwm_bucket + 10, 200, 10):
            v = self.e.get(f't3|{loco}|{b}')
            if v and v['n'] >= 5:
                return 305.0 / max(1, v['fast_bound'])
        return self.vmax_global.get(loco, 0.5)

Report = namedtuple('Report', 'events confirmations contradictions lost_full_circle')

class Navigator:
    CONFIRM_N = 3

    def __init__(self, env, loco, sdir, dna, spacing):
        self.env, self.loco, self.sdir = env, loco, sdir
        self.dna, self.spc = dna, spacing
        self.step = 1 if sdir == 'CW' else -1
        self.anchor = None          # (mm, ts)
        self.hi = 0.0               # corridor fast side, mm since anchor
        self.P = set()              # positions possibly occupied now
        self.pending = None         # held doubtful event
        self.consec = 0
        self.state = 'UNSET'
        self.log = []
        self.polwin = []            # last W observed poles for re-acquisition
        self.reacq = None           # (candidate mm, consecutive count)

    def circuit_mm(self):
        return sum(self.spc)

    def interval_ending_at(self, mm):
        return (mm - 1) % DNA_N if self.sdir == 'CW' else mm % DNA_N

    def dist(self, a, b):
        """track mm from a forward (in travel direction) to b."""
        d, mm = 0, a
        for _ in range(DNA_N):
            if mm == b: return d
            nxt = (mm + self.step) % DNA_N
            d += self.spc[self.interval_ending_at(nxt)]
            mm = nxt
        return d

    def declare(self, mm, ts):
        self.polwin = []
        self.reacq = None
        self.anchor = (mm % DNA_N, ts)
        self.hi = 0.0
        self.P = {mm % DNA_N}
        self.pending = None
        self.consec = 0
        self.state = 'NORMAL'
        self.log.append(dict(t=ts, ev='DECLARED', mm=mm % DNA_N))

    def advance_corridor(self, pwm_hist, now, last_ts):
        """Integrate hi over [last_ts, now] using the recorded pwm history."""
        if self.anchor is None: return
        samples = sorted((now - off/1000.0, v) for off, v in (pwm_hist or []))
        t = last_ts
        cur = samples[0][1] if samples else 0
        zero_run = getattr(self, '_zero_run', 0.0)
        for st_t, v in samples + [(now, None)]:
            if st_t > t:
                span = st_t - t
                if cur <= 20:
                    # coast allowance for the first 5 s of a dead-zone
                    # stretch, then physically parked
                    coast = max(0.0, min(span, 5.0 - zero_run))
                    self.hi += self.env.vmax(self.loco, 0) * coast * 1000.0
                    zero_run += span
                else:
                    self.hi += self.env.vmax(self.loco, (cur//10)*10) * span * 1000.0
                    zero_run = 0.0
                t = st_t
            if v is not None: cur = v
        self._zero_run = zero_run
        self.hi = min(self.hi, self.circuit_mm() + 1)

    def candidates(self):
        """all positions ahead of anchor within the corridor's fast side."""
        out, mm, d = [], self.anchor[0], 0
        for _ in range(DNA_N):
            nxt = (mm + self.step) % DNA_N
            d += self.spc[self.interval_ending_at(nxt)]
            if d > self.hi + 30: break
            out.append(nxt)
            mm = nxt
        return out

    def _match(self, frm, ev, dt_ms):
        """positions reachable from frm that this event could confirm."""
        ok = []
        reach = set(self.candidates())
        mm, d, steps = frm, 0, 0
        for _ in range(DNA_N):
            nxt = (mm + self.step) % DNA_N
            d += self.spc[self.interval_ending_at(nxt)]
            steps += 1
            if nxt not in reach and steps > 1: break
            if nxt in reach and self.dna[nxt] == (1 if ev['polarity']=='N' else 0):
                # timing: dt must not be faster than the summed fast bounds
                fb = 0
                m2, s2 = frm, 0
                while m2 != nxt and s2 <= steps:
                    n2 = (m2 + self.step) % DNA_N
                    fb += self.env.fast_dt(self.loco, self.sdir,
                                           self.interval_ending_at(n2),
                                           ((ev.get('pwm_actual') or 0)//10)*10,
                                           self.spc[self.interval_ending_at(n2)])
                    m2, s2 = n2, s2+1
                if dt_ms is None or dt_ms >= fb:
                    ok.append(nxt)
            mm = nxt
        return ok

    def on_event(self, ev, last_ts):
        ts = ev['ts']
        self.advance_corridor(ev.get('pwm_actual_history'), ts, last_ts)
        if self.anchor is None: return
        if self.hi >= self.circuit_mm():
            if self.state != 'LOST_FULL_CIRCLE':
                self.state = 'LOST_FULL_CIRCLE'
                self.log.append(dict(t=ts, ev='LOST_FULL_CIRCLE'))
            # Route-wide matching is PERMITTED now and only now: a full
            # circuit is physically reachable. Windowed unique DNA match,
            # three consistent hits, then re-anchor.
            self.polwin = (self.polwin + [1 if ev['polarity']=='N' else 0])[-12:]
            if len(self.polwin) == 12:
                hits = [k for k in range(DNA_N)
                        if all(self.dna[(k - i*self.step) % DNA_N] == pv
                               for i, pv in enumerate(reversed(self.polwin)))]
                if len(hits) == 1:
                    want = (self.reacq[0] + self.step) % DNA_N if self.reacq else None
                    if self.reacq and hits[0] == want:
                        self.reacq = (hits[0], self.reacq[1] + 1)
                    else:
                        self.reacq = (hits[0], 1)
                    if self.reacq[1] >= self.CONFIRM_N:
                        self.log.append(dict(t=ts, ev='REACQUIRED', mm=hits[0]))
                        self.declare(hits[0], ts)
                        self._zero_run = 0.0
                        return
                else:
                    self.reacq = None
            return

        dt = ev.get('dt_ms')
        newP = set()
        for p in self.P:
            newP.update(self._match(p, ev, dt))
        if self.pending is not None:
            # pending-genuine: pending advanced us; this event continues from
            # its matches. pending-phantom: fold its dt into this event.
            pg, pdts = self.pending
            for p in pg: newP.update(self._match(p, ev, dt))
            for p in self.P:
                newP.update(self._match(p, ev, (dt or 0) + (pdts or 0)))

        if not newP:
            # No marker is yet reachable at all: an event arriving before the
            # corridor's fast side reaches the NEXT magnet cannot be genuine.
            # That is the phantom signature - hold it (its dt folds into the
            # successor via the pending mechanism) and do not move. It is NOT
            # a contradiction: contradiction is reserved for reachable
            # candidates that the evidence eliminates.
            nxt_spacing = self.spc[self.interval_ending_at(
                (next(iter(self.P)) + self.step) % DNA_N)] if self.P else 305
            if self.hi + 30 < nxt_spacing:
                self.pending = (set(), (self.pending[1] if self.pending else 0)
                                + (dt or 0))
                self.log.append(dict(t=ts, ev='PHANTOM_SUSPECT', hi=round(self.hi)))
                return
            gm = self._all_pol_matches(ev)
            if self.pending is None and gm is not None:
                self.pending = (gm, dt)      # HOLD, do not advance, do not discard
                self.log.append(dict(t=ts, ev='PENDING', mm_set=sorted(gm)[:6]))
                return
            self.state = 'CONTRADICTION'
            self.log.append(dict(t=ts, ev='CONTRADICTION',
                                 P=sorted(self.P)[:8], hi=round(self.hi)))
            return

        self.pending = None
        self.P = newP
        self.log.append(dict(t=ts, ev='STEP', P=sorted(self.P)[:8]))
        if len(self.P) == 1:
            self.consec += 1
            if self.consec >= self.CONFIRM_N:
                mm = next(iter(self.P))
                self.declare(mm, ts)
                self.log[-1] = dict(t=ts, ev='CONFIRMED', mm=mm)
        else:
            self.consec = 0

    def _all_pol_matches(self, ev):
        want = 1 if ev['polarity'] == 'N' else 0
        c = [k for k in self.candidates() if self.dna[k] == want]
        return set(c) if c else None

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--records', required=True)
    ap.add_argument('--db', required=True)
    ap.add_argument('--loco', required=True)
    ap.add_argument('--session', required=True, help='<source-substr>:boot<N>')
    ap.add_argument('--report')
    ap.add_argument('--continue-analysis', action='store_true',
                    help='after a contradiction, re-seed from the FIRMWARE '
                         'label and keep scanning. Every such position is '
                         'externally supplied and labeled EXTERNAL_RESEED; '
                         'the run no longer measures navigator performance '
                         'past the first contradiction. Default: strict - '
                         'stop at the first contradiction.')
    args = ap.parse_args()

    import re, pathlib
    src = open(pathlib.Path(__file__).resolve().parents[2]
               / 'firmware/QUORUM/QUORUM.ino').read()
    dna = [int(x) for x in re.findall(r'\d+',
        src.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    spc = [int(x) for x in re.findall(r'\d+',
        src.split('static const uint16_t spacingMm[DNA_N] PROGMEM = {')[1].split('};')[0])]

    srcpat, boot = args.session.rsplit(':boot', 1)
    rows = [json.loads(l) for l in open(args.records)]
    rows = [r for r in rows if r['loco'] == args.loco
            and srcpat in r['source'] and r['boot'] == int(boot)]
    rows.sort(key=lambda r: r['ts'])
    if not rows: sys.exit('no records for that session')

    env = Envelopes(json.load(open(args.db)))
    nav = Navigator(env, args.loco, rows[0].get('session_dir') or 'CW', dna, spc)
    nav.declare(rows[0]['mm'], rows[0]['ts'])   # seed at first firmware label
    last = rows[0]['ts']
    stopped = None
    cur_dir = rows[0].get('session_dir') or 'CW'
    contras = external = 0
    stopped = None
    for r in rows[1:]:
        d = r.get('session_dir') or cur_dir
        if d != cur_dir:
            # direction change: the corridor model is direction-anchored.
            # The seed position comes from the FIRMWARE label and is marked
            # as externally supplied.
            cur_dir = d
            nav.sdir = d; nav.step = 1 if d == 'CW' else -1
            nav.log.append(dict(t=r['ts'], ev='EXTERNAL_RESEED',
                                source='firmware_label_on_reversal', mm=r['mm']))
            external += 1
            nav.declare(r['mm'], r['ts']); last = r['ts']; continue
        nav.on_event(r, last)
        last = r['ts']
        if nav.state == 'CONTRADICTION':
            contras += 1
            if not args.continue_analysis:
                stopped = r
                break
            nav.log.append(dict(t=r['ts'], ev='EXTERNAL_RESEED',
                                source='firmware_label_after_contradiction',
                                mm=r['mm']))
            external += 1
            nav.declare(r['mm'], r['ts'])

    conf = sum(1 for l in nav.log if l['ev'] == 'CONFIRMED')
    contra = sum(1 for l in nav.log if l['ev'] == 'CONTRADICTION')
    lost = sum(1 for l in nav.log if l['ev'] == 'LOST_FULL_CIRCLE')
    mode = 'CONTINUE-ANALYSIS' if args.continue_analysis else 'STRICT'
    final = ('STOPPED_AT_FIRST_CONTRADICTION' if (stopped is not None)
             else nav.state)
    print(f'[{mode}] events {len(rows)} | confirmations {conf} | pending-held '
          f'{sum(1 for l in nav.log if l["ev"]=="PENDING")} | '
          f'contradictions {contra} | lost_full_circle {lost} | '
          f'externally-seeded positions {external} | final {final}')
    if args.continue_analysis and external:
        print(f'NOTE: {external} positions were supplied by firmware labels, '
              f'not by the navigator. This run does not measure navigator '
              f'performance past the first contradiction.')
    if args.report:
        json.dump(dict(session=args.session, loco=args.loco,
                       mode=mode, final=final,
                       externally_seeded=external,
                       log=nav.log), open(args.report, 'w'), indent=1)
        print('report ->', args.report)

if __name__ == '__main__':
    sys.exit(main())
