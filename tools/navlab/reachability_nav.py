#!/usr/bin/env python3
"""navlab artifact 3, iteration 2: the forward-reachability navigator.

Iteration-2 corrections (operator directive, 2026-08-22):
  1. REVERSALS ARE NATIVE. Effective travel direction is derived from
     motor_dir x session_dir per event (14 motor flips verified in Otto's
     held-out session). On a flip the position hypotheses are PRESERVED and
     their direction of travel reversed: the anchor moves to the rearmost
     hypothesis in the NEW direction and the corridor is re-opened over the
     hypothesis span. No firmware position label is consulted.
  2. THE CORRIDOR ADVANCES ON PHYSICAL TIME: the marker's own dt (measured
     on-device between events), distributed over the recorded PWM history.
     MQTT receipt-time gaps are never treated as travel time. A saturated dt
     (65535) advances by the saturation floor only.
  3. Envelope lookups take the LOOSEST fast bound among adequately-populated
     tiers for the effective direction - a thin tier-1 no longer silently
     overrides broader measurement (and never resolves a miss by itself).
  4. An observation only SLIGHTLY faster than the empirical envelope
     (within MARGINAL_SLACK) retains the genuine-marker hypothesis as
     PENDING for its successor to arbitrate, instead of being eliminated.

Unchanged doctrine: hypotheses only ever move forward in the current travel
direction; doubtful events are held, never discarded; CONTRADICTION stops and
reports; route-wide matching only once a full circuit is physically
reachable. Host tool only - no firmware flag, no MQTT, no Pi contract.

Usage:
  python3 tools/navlab/reachability_nav.py --records <records.jsonl>
      --db <timing_db.json> --loco 9950012 --session <source:bootN>
      [--report out.json] [--continue-analysis]
"""
import argparse, json, sys

DNA_N = 171
MARGINAL_SLACK = 0.25

class Envelopes:
    MIN_N = 8
    def __init__(self, db):
        self.e = db['envelopes']
        self.vmax_global = {}
        for k, v in self.e.items():
            p = k.split('|')
            if p[0] == 't3' and v['n'] >= self.MIN_N:
                vm = 305.0 / max(1, v['fast_bound'])
                self.vmax_global[p[1]] = max(self.vmax_global.get(p[1], 0), vm)

    def fast_dt(self, loco, eff_dir, interval, pwm_bucket, spacing):
        """Fastest credible ms for ONE interval: loosest adequately-populated
        tier (correction 3 - a thin tier-1 must not override)."""
        cands = []
        for key in (f't1|{loco}|{eff_dir}|{interval}|{pwm_bucket}',
                    f't2|{loco}|{eff_dir}|{pwm_bucket}',
                    f't3|{loco}|{pwm_bucket}'):
            v = self.e.get(key)
            if v and v['n'] >= self.MIN_N:
                cands.append(v['fast_bound'])
        if cands:
            return min(cands)
        vm = self.vmax_global.get(loco)
        return spacing / vm if vm else 0

    def vmax(self, loco, pwm_bucket):
        if pwm_bucket <= 20:
            return 0.15                      # coast/push ceiling, dead zone
        v = self.e.get(f't3|{loco}|{pwm_bucket}')
        if v and v['n'] >= self.MIN_N:
            return 305.0 / max(1, v['fast_bound'])
        for b in range(pwm_bucket + 10, 200, 10):
            v = self.e.get(f't3|{loco}|{b}')
            if v and v['n'] >= self.MIN_N:
                return 305.0 / max(1, v['fast_bound'])
        return self.vmax_global.get(loco, 0.5)

class Navigator:
    CONFIRM_N = 3

    def __init__(self, env, loco, dna, spacing):
        self.env, self.loco = env, loco
        self.dna, self.spc = dna, spacing
        self.step = +1                  # effective direction; set per event
        self.eff = None                 # (session_dir, motor_dir) last seen
        self.anchor = None
        self.hi = 0.0
        self.P = set()
        self.pending = None             # (candidate set, accumulated dt)
        self.consec = 0
        self.state = 'UNSET'
        self.log = []
        self.polwin = []; self.reacq = None
        self._zero_run = 0.0

    # ---- geometry -----------------------------------------------------
    def circuit_mm(self): return sum(self.spc)
    def eff_dir(self): return 'CW' if self.step > 0 else 'CCW'
    def interval_ending_at(self, mm):
        return (mm - 1) % DNA_N if self.step > 0 else mm % DNA_N
    def dist(self, a, b):
        d, mm = 0, a
        for _ in range(DNA_N):
            if mm == b: return d
            nxt = (mm + self.step) % DNA_N
            d += self.spc[self.interval_ending_at(nxt)]
            mm = nxt
        return d

    # ---- state changes -------------------------------------------------
    def declare(self, mm, ts):
        self.anchor = (mm % DNA_N, ts)
        self.hi = 0.0
        self.P = {mm % DNA_N}
        self.pending = None
        self.consec = 0
        self.polwin = []; self.reacq = None
        self.state = 'NORMAL'
        self.log.append(dict(t=ts, ev='DECLARED', mm=mm % DNA_N))

    def maybe_reverse(self, ev):
        sd = ev.get('session_dir') or 'CW'
        md = ev.get('motor_dir')
        md = md if md in ('FWD', 'REV') else (self.eff[1] if self.eff else 'FWD')
        new = (+1 if sd == 'CW' else -1) * (+1 if md == 'FWD' else -1)
        if self.eff is None:
            self.eff = (sd, md); self.step = new; return
        if new == self.step:
            self.eff = (sd, md); return
        # REVERSAL: preserve hypotheses, reverse their direction of travel.
        # No firmware label is consulted (iteration-2 correction 1).
        old_step, self.step, self.eff = -new, new, (sd, md)
        if self.P:
            best, span = None, None
            for a in self.P:
                sp = max(self.dist(a, q) for q in self.P)
                if span is None or sp < span:
                    best, span = a, sp
            self.anchor = (best, ev['ts'])
            self.hi = float(span)
        self.pending = None
        self.polwin = []; self.reacq = None
        self.consec = 0
        self._zero_run = 0.0
        self.log.append(dict(t=ev['ts'], ev='REVERSAL',
                             to=self.eff_dir(), P=sorted(self.P)[:8],
                             anchor=self.anchor[0] if self.anchor else None,
                             hi=round(self.hi)))

    # ---- corridor (correction 2: physical dt, never receipt gaps) ------
    def advance_corridor(self, ev):
        if self.anchor is None: return
        dt = ev.get('dt_ms')
        elapsed = float(min(dt, 65535)) if dt and dt > 0 else 0.0
        if elapsed <= 0: return
        # The profile CONCURRENT with this dt is the tail of the history
        # whose offsets fall inside the dt window (+1.5 s sampling slack).
        # The full 45 s window is dominated by whatever preceded the motion
        # (a pre-departure standstill starves the corridor and strangles
        # genuine markers - found on the first iteration-2 strict run).
        dt_win = (ev.get('dt_ms') or 0) + 1500
        hist = [(o, v) for o, v in (ev.get('pwm_actual_history') or [])
                if o <= dt_win]
        segs = sorted(hist, key=lambda x: -x[0])       # oldest first
        if not segs:
            segs = [[0, ev.get('pwm_actual') or 0]]
        # distribute the physical elapsed time across the recorded profile,
        # weighting by the history's own relative spans
        offs = [o for o, _ in segs]
        spans = []
        for i, (o, v) in enumerate(segs):
            nxt = offs[i + 1] if i + 1 < len(segs) else 0
            spans.append((max(1.0, o - nxt), v))
        total = sum(w for w, _ in spans)
        z = self._zero_run
        for w, v in spans:
            ms = elapsed * (w / total)
            if v <= 20:
                coast = max(0.0, min(ms, 5000.0 - z))
                self.hi += self.env.vmax(self.loco, 0) * coast
                z += ms
            else:
                self.hi += self.env.vmax(self.loco, (v // 10) * 10) * ms
                z = 0.0
        self._zero_run = z
        self.hi = min(self.hi, self.circuit_mm() + 1)

    # ---- candidate machinery -------------------------------------------
    def candidates(self):
        out, mm, d = [], self.anchor[0], 0
        for _ in range(DNA_N):
            nxt = (mm + self.step) % DNA_N
            d += self.spc[self.interval_ending_at(nxt)]
            if d > self.hi + 30: break
            out.append(nxt)
            mm = nxt
        return out

    def _match(self, frm, ev, dt_ms):
        """returns (ok, marginal): positions reachable from frm this event
        could confirm; marginal = only-slightly-too-fast (correction 4)."""
        ok, marginal = [], []
        reach = set(self.candidates())
        want = 1 if ev['polarity'] == 'N' else 0
        pb = ((ev.get('pwm_actual') or 0) // 10) * 10
        mm, steps = frm, 0
        fb = 0.0
        for _ in range(DNA_N):
            nxt = (mm + self.step) % DNA_N
            iv = self.interval_ending_at(nxt)
            fb += self.env.fast_dt(self.loco, self.eff_dir(), iv, pb, self.spc[iv])
            steps += 1
            if nxt not in reach and steps > 1: break
            if nxt in reach and self.dna[nxt] == want:
                if dt_ms is None or dt_ms >= fb:
                    ok.append(nxt)
                elif dt_ms >= fb * (1 - MARGINAL_SLACK):
                    marginal.append(nxt)
            mm = nxt
        return ok, marginal

    def _all_pol_matches(self, ev):
        want = 1 if ev['polarity'] == 'N' else 0
        c = [k for k in self.candidates() if self.dna[k] == want]
        return set(c) if c else None

    # ---- the event ------------------------------------------------------
    def on_event(self, ev):
        ts = ev['ts']
        self.maybe_reverse(ev)
        self.advance_corridor(ev)
        if self.anchor is None: return
        if self.hi >= self.circuit_mm():
            if self.state != 'LOST_FULL_CIRCLE':
                self.state = 'LOST_FULL_CIRCLE'
                self.log.append(dict(t=ts, ev='LOST_FULL_CIRCLE'))
            self.polwin = (self.polwin + [1 if ev['polarity'] == 'N' else 0])[-12:]
            if len(self.polwin) == 12:
                hits = [k for k in range(DNA_N)
                        if all(self.dna[(k - i * self.step) % DNA_N] == pv
                               for i, pv in enumerate(reversed(self.polwin)))]
                if len(hits) == 1:
                    want = (self.reacq[0] + self.step) % DNA_N if self.reacq else None
                    self.reacq = ((hits[0], self.reacq[1] + 1)
                                  if self.reacq and hits[0] == want
                                  else (hits[0], 1))
                    if self.reacq[1] >= self.CONFIRM_N:
                        self.log.append(dict(t=ts, ev='REACQUIRED', mm=hits[0]))
                        self.declare(hits[0], ts)
                        self._zero_run = 0.0
                        return
                else:
                    self.reacq = None
            return

        dt = ev.get('dt_ms')
        if not dt or dt <= 0:
            # dt-chain reset: traversal time UNKNOWN (docs/NAVLAB_DT0_SEMANTICS.md).
            # Grant exactly ONE interval of corridor, skip the timing veto,
            # keep both the same-magnet-reread and the advance hypotheses.
            want = 1 if ev['polarity'] == 'N' else 0
            grant = self.spc[self.interval_ending_at(
                (max(self.P, key=lambda p: self.dist(self.anchor[0], p))
                 + self.step) % DNA_N)] if self.P else 305
            self.hi = min(self.hi + grant, self.circuit_mm() + 1)
            stay = {p for p in self.P if self.dna[p] == want}
            reach = set(self.candidates())
            adv = set()
            for p in self.P:
                mm = p
                for _ in range(DNA_N):
                    nxt = (mm + self.step) % DNA_N
                    if nxt not in reach: break
                    if self.dna[nxt] == want: adv.add(nxt)
                    mm = nxt
            hyp = stay | adv
            if hyp:
                self.pending = None
                self.P = hyp
                self.consec = 0        # a reset event never counts toward confirmation
                self.log.append(dict(t=ts, ev='DT_RESET', P=sorted(self.P)[:8],
                                     hi=round(self.hi), anchor=self.anchor[0]))
            else:
                self.pending = (set(), 0)
                self.log.append(dict(t=ts, ev='PHANTOM_SUSPECT', hi=round(self.hi)))
            return

        newP, newM = set(), set()
        for p in self.P:
            o, m = self._match(p, ev, dt)
            newP.update(o); newM.update(m)
        if self.pending is not None:
            pg, pdt = self.pending
            for p in pg:
                o, m = self._match(p, ev, dt)
                newP.update(o); newM.update(m)
            fold = (dt or 0) + (pdt or 0)
            for p in self.P:
                o, m = self._match(p, ev, fold)
                newP.update(o); newM.update(m)

        if not newP:
            if newM:
                # correction 4: only-slightly-too-fast keeps the genuine
                # hypothesis alive as pending
                self.pending = (newM, dt)
                self.log.append(dict(t=ts, ev='PENDING_MARGINAL',
                                     mm_set=sorted(newM)[:6]))
                return
            nxt_spacing = (self.spc[self.interval_ending_at(
                (next(iter(self.P)) + self.step) % DNA_N)] if self.P else 305)
            if self.hi + 30 < nxt_spacing:
                self.pending = (set(), (self.pending[1] if self.pending else 0)
                                + (dt or 0))
                self.log.append(dict(t=ts, ev='PHANTOM_SUSPECT', hi=round(self.hi)))
                return
            gm = self._all_pol_matches(ev)
            if self.pending is None and gm is not None:
                self.pending = (gm, dt)
                self.log.append(dict(t=ts, ev='PENDING', mm_set=sorted(gm)[:6]))
                return
            self.state = 'CONTRADICTION'
            self.log.append(dict(t=ts, ev='CONTRADICTION',
                                 P=sorted(self.P)[:8], hi=round(self.hi),
                                 anchor=self.anchor[0]))
            return

        self.pending = None
        self.P = newP
        self.log.append(dict(t=ts, ev='STEP', P=sorted(self.P)[:8],
                             hi=round(self.hi), anchor=self.anchor[0]))
        if len(self.P) == 1:
            self.consec += 1
            if self.consec >= self.CONFIRM_N:
                mm = next(iter(self.P))
                pre_anchor, pre_hi = self.anchor[0], round(self.hi)
                self.declare(mm, ts)
                # anchor/hi here are the PRE-confirmation values, so the
                # acceptance checker can validate this position against the
                # corridor that admitted it; the anchor then moves to mm.
                self.log[-1] = dict(t=ts, ev='CONFIRMED', mm=mm,
                                    dir=self.eff_dir(),
                                    anchor=pre_anchor, hi=pre_hi)
        else:
            self.consec = 0

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--records', required=True)
    ap.add_argument('--db', required=True)
    ap.add_argument('--loco', required=True)
    ap.add_argument('--session', required=True)
    ap.add_argument('--report')
    ap.add_argument('--continue-analysis', action='store_true',
                    help='after a contradiction, re-seed from the FIRMWARE '
                         'label and keep scanning; every such position is '
                         'labeled EXTERNAL_RESEED and the stored final state '
                         'is ANALYSIS_ONLY. Never navigator performance.')
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
    nav = Navigator(env, args.loco, dna, spc)
    nav.maybe_reverse(rows[0])
    nav.declare(rows[0]['mm'], rows[0]['ts'])
    contras = external = 0
    stopped = None
    for r in rows[1:]:
        nav.on_event(r)
        if nav.state == 'CONTRADICTION':
            contras += 1
            if not args.continue_analysis:
                stopped = r; break
            nav.log.append(dict(t=r['ts'], ev='EXTERNAL_RESEED',
                                source='firmware_label_after_contradiction',
                                mm=r['mm']))
            external += 1
            nav.declare(r['mm'], r['ts'])

    conf = sum(1 for l in nav.log if l['ev'] == 'CONFIRMED')
    contra = sum(1 for l in nav.log if l['ev'] == 'CONTRADICTION')
    lost = sum(1 for l in nav.log if l['ev'] == 'LOST_FULL_CIRCLE')
    rev = sum(1 for l in nav.log if l['ev'] == 'REVERSAL')
    pend = sum(1 for l in nav.log if l['ev'] in ('PENDING', 'PENDING_MARGINAL'))
    mode = 'CONTINUE-ANALYSIS' if args.continue_analysis else 'STRICT'
    if stopped is not None:
        final = 'STOPPED_AT_FIRST_CONTRADICTION'
    elif args.continue_analysis and external:
        final = f'ANALYSIS_ONLY_{external}_EXTERNAL_SEEDS'
    else:
        final = nav.state
    print(f'[{mode}] events {len(rows)} | confirmations {conf} | reversals '
          f'{rev} | pending-held {pend} | phantom-suspect '
          f'{sum(1 for l in nav.log if l["ev"]=="PHANTOM_SUSPECT")} | '
          f'contradictions {contra} | lost_full_circle {lost} | '
          f'externally-seeded {external} | final {final}')
    if args.report:
        json.dump(dict(session=args.session, loco=args.loco, mode=mode,
                       final=final, externally_seeded=external,
                       log=nav.log), open(args.report, 'w'), indent=1)
        print('report ->', args.report)

if __name__ == '__main__':
    sys.exit(main())
