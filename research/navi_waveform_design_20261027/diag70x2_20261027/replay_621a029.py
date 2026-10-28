#!/usr/bin/env python3
"""Host replay of the 70x2 detection path of NAVI_POWER_OF_NORMAL_MANUAL_ONLY at 621a029. Research only.

621a029 never ran on a locomotive as far as the logs show: no MQTT log in Pi_Log_Downloads carries its
state/nav fields timing_lower_ms / timing_calibration_ready except the Oct 27 afternoon NAVI_WAVEFORM_0_1
boots (which inherited them). There is therefore no firmware output to check this replay against; it
implements the code as written. Sources: firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY at
621a029 (byte-identical copies checked).

Ported:
  NaviObservation.h  20 ms blocks on the boot clock (us // 20000): mean of the block's raw reads; NaN if
                     < 50 reads or the block touches a serial jump. pwm = max applied PWM in the block.
  NaviBaseline.h     Boot: append every PWM==0 block, live median. onConfirmedOpening(): (Boot|Collect)
                     recompute, then AwaitReturn. AwaitReturn/ConfirmReturn: 10 consecutive blocks within
                     20 counts of the retained level -> Collect (count reset). Collect: append PWM>0
                     blocks, live median. freezeAtGate(): recompute if fresh, phase Frozen (skipped in Boot).
                     APPROXIMATION: the firmware's buffer is reordered in place by nth_element and then
                     overwritten at count % 2048; this replay keeps a time-ordered ring. The two differ only
                     once a phase has collected more than 2048 blocks (41 s).
  NaviCandidate.h    70x2 rule, stride 5 counted over calls to onSample(); onSample() is called only while
                     baseline ready AND identification gate open; reset() on every read while the gate is shut.
  NaviTarget.h       declare / suspend; windowOpen(now): Tracking and (no anchor, or anchor PWM == 90 and
                     now - anchor >= lowerFactor * survey base (CW 1072.8 ms, CCW 1098.9 ms) for the
                     (last, dir, PWM 90) profile); onOnset: Confirmed iff polarity == expected next AND
                     (no anchor, or onset PWM == 90 and windowOpen(onset time)). Else NonQualifying.
  .ino rvmTask       per block: control -> drain onsets (judge only if gate open and windowOpen(openUs);
                     a confirmation shuts the gate, later queued onsets are dropped) -> baseline.onBlock ->
                     if windowOpen(now) and gate shut: freezeAtGate (not in Boot) -> gate = open && ready.
                     APPROXIMATION: rvmTask runs when a block is emitted (the first read of the next block);
                     here the gate state set at the end of block k governs every read of block k+1.
  Commands           from the capture's ACTION records (kind 2 = applied): session_direction sets the
                     session direction; start_interval declares (refused, as admitStartMarker does, when
                     no session direction is set). Motor direction is forward in every driven run.

Usage: python3 replay_621a029.py <oct27 npz dir> <fw621 dir> [out.json]
"""
import collections
import json
import re
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf                 # noqa: E402
import replay_bank as rb  # noqa: E402
import replay_70x2 as r7  # noqa: E402

THR, STRIDE, CAP, ENTRY, CONFIRM = 70.0, 5, 2048, 20.0, 10
BASE = {+1: 1072.8, -1: 1098.9}


def load_fw(fw):
    rm = (Path(fw) / 'RouteMap.h').read_text()
    pol = [int(x) for x in re.search(r'ROUTE_POLARITY\[ROUTE_N\] = \{(.*?)\};', rm, re.S).group(1).replace('\n', '').split(',') if x.strip()]
    prof = {}
    for m in re.finditer(r'\{(\d+), ([+-]1), 90, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f, \d+\}',
                         (Path(fw) / 'NaviTimingProfiles.h').read_text()):
        prof[(int(m.group(1)), int(m.group(2)))] = float(m.group(4))
    return pol, prof


class Baseline:
    def __init__(self):
        self.phase, self.buf, self.level, self.ready, self.confirm, self.count = 'Boot', collections.deque(maxlen=CAP), 0.0, False, 0, 0

    def _recompute(self):
        if self.buf:
            v = sorted(self.buf)
            self.level, self.ready = float(v[len(v) // 2]), True

    def _append(self, v):
        self.buf.append(v); self.count += 1; self._recompute()

    def on_block(self, h, pwm_on):
        if not np.isfinite(h):
            if self.phase == 'ConfirmReturn':
                self.confirm = 0
            return
        if self.phase == 'Boot':
            if not pwm_on:
                self._append(h)
            return
        if self.phase in ('AwaitReturn', 'ConfirmReturn'):
            if abs(h - self.level) <= ENTRY:
                self.confirm += 1
                if self.confirm >= CONFIRM:
                    self.phase, self.count, self.confirm = 'Collect', 0, 0
                    self.buf.clear()
                else:
                    self.phase = 'ConfirmReturn'
            else:
                self.confirm, self.phase = 0, 'AwaitReturn'
            return
        if self.phase == 'Collect' and pwm_on:
            self._append(h)

    def on_confirmed_opening(self):
        if self.phase in ('Boot', 'Collect'):
            self._recompute()
        self.phase, self.confirm, self.count = 'AwaitReturn', 0, 0
        self.buf.clear()

    def freeze_at_gate(self):
        fresh = self.phase == 'Collect' and self.count != 0
        if fresh:
            self._recompute()
        if self.phase != 'Boot':
            self.phase, self.confirm, self.count = 'Frozen', 0, 0
            self.buf.clear()
        return fresh


class Target:
    def __init__(self, pol, prof):
        self.pol, self.prof = pol, prof
        self.tracking, self.last, self.next, self.dir = False, 0, 0, 0
        self.anchor, self.anchor_us, self.anchor_pwm = False, 0, 0

    def declare(self, last, nxt, d):
        self.tracking, self.last, self.next, self.dir = True, last, nxt, d
        self.anchor, self.anchor_us, self.anchor_pwm = False, 0, 0

    def lower_ms(self):
        f = self.prof.get((self.last, self.dir))
        return int(f * BASE[self.dir] + 0.5) if f else 0

    def window_open(self, now):
        if not self.tracking:
            return False
        if not self.anchor:
            return True
        if self.anchor_pwm != 90 or now < self.anchor_us:
            return False
        lo = self.lower_ms()
        return lo != 0 and now - self.anchor_us >= lo * 1000

    def on_onset(self, o):
        if not self.tracking:
            return 'NoClaim', None
        timing_ok = not self.anchor or (o['pwm'] == 90 and self.window_open(o['us']))
        exp = self.next
        if o['pol'] != (1 if self.pol[exp] else -1) or not timing_ok:
            return 'NonQualifying', exp
        self.anchor, self.anchor_us, self.anchor_pwm = True, o['us'], o['pwm']
        self.last, self.next = exp, (exp + self.dir) % len(self.pol)
        return 'Confirmed', exp


def replay(native, actions, pol, prof):
    us = native['t_us'].astype(np.int64); raw = native['raw'].astype(np.int64)
    ser = native['serial'].astype(np.int64); pwm = native['pwm'].astype(np.int64)
    blk = us // 20000
    edges = np.flatnonzero(np.diff(blk)) + 1
    starts = np.r_[0, edges]; ends = np.r_[edges, len(us)]
    jump = np.r_[False, np.diff(ser) != 1]
    acts = sorted((a['t_us'], a['topic'].split('/')[-1], a['payload']) for a in actions
                  if a['kind'] == 2 and a['topic'].endswith(('session_direction', 'start_interval', 'direction')))
    ai = 0
    base, tgt = Baseline(), Target(pol, prof)
    gate, session_dir = False, 0
    stride = 0; pending = active = False; psign = 1
    judged, raw_onsets, gate_spans, gate_on_at = [], 0, [], None
    t0 = int(us[0])
    for s, e in zip(starts, ends):
        # hallTask: reads of this block under the gate/level set at the end of the previous block
        q = []
        level = base.level
        for k in range(s, e):
            if not gate:
                pending = active = False
                continue
            if not base.ready:
                continue
            ev = stride % STRIDE == 0; stride += 1
            if not ev:
                continue
            dev = float(raw[k]) - level
            if abs(dev) >= THR:
                sign = 1 if dev >= 0 else -1
                if not active and pending and psign == sign:
                    q.append(dict(us=int(us[k]), pol=sign, pwm=int(pwm[k]), level=level, k=k)); active = True
                pending, psign = True, sign
            else:
                pending = active = False
        raw_onsets += len(q)
        now = int(us[e - 1])
        # rvmTask: control
        while ai < len(acts) and acts[ai][0] <= now:
            _, leaf, p = acts[ai]; ai += 1
            if leaf == 'session_direction':
                nd = 1 if p == 'CW' else -1
                if nd != session_dir and tgt.tracking:
                    tgt.tracking = False; gate = False
                session_dir = nd
            elif leaf == 'start_interval' and session_dir:
                a, b = (int(x) for x in p.split('-'))
                gate = False
                tgt.declare(a if session_dir > 0 else b, b if session_dir > 0 else a, session_dir)
            elif leaf == 'direction':
                if tgt.tracking:
                    tgt.tracking = False; gate = False
        # drain onsets
        for o in q:
            if gate and tgt.window_open(o['us']):
                res, exp = tgt.on_onset(o)
                o.update(result=res, expected=exp, t=(o['us'] - t0) / 1e6)
                judged.append(o)
                if res == 'Confirmed':
                    base.on_confirmed_opening(); gate = False
        seg = raw[s:e]
        bad = jump[s:e].any() or (e < len(us) and jump[e])
        h = float(seg.mean()) if (e - s) >= 50 and not bad else float('nan')
        base.on_block(h, int(pwm[s:e].max()) > 0)
        op = tgt.window_open(now)
        if op and not gate and base.phase != 'Boot':
            base.freeze_at_gate()
        newgate = op and base.ready
        if newgate and gate_on_at is None:
            gate_on_at = now
        if not newgate and gate_on_at is not None:
            gate_spans.append(((gate_on_at - t0) / 1e6, (now - t0) / 1e6)); gate_on_at = None
        gate = newgate
    if gate_on_at is not None:
        gate_spans.append(((gate_on_at - t0) / 1e6, (int(us[-1]) - t0) / 1e6))
    return judged, raw_onsets, gate_spans


def main():
    oct27, fw = Path(sys.argv[1]), sys.argv[2]
    pol, prof = load_fw(fw)
    assert len(pol) == 171 and len(prof) > 300, (len(pol), len(prof))
    res = {}
    for p in sorted(oct27.glob('*.npz')):
        boot = p.stem.split('_')[3]
        if boot not in rb.DRIVEN27:
            continue
        native = np.load(p)['native']
        actions = json.loads(p.with_suffix('.json').read_text())['actions']
        judged, n_raw, spans = replay(native, actions, pol, prof)
        tr = mf.condition(native)
        tru, _ = rb.truth27(tr, boot)
        tt = np.array([x[0] for x in tru])
        conf = [j for j in judged if j['result'] == 'Confirmed']
        # physical correctness of each confirmation: nearest magnet passage within (-0.15, +0.6) s of onset
        def phys(j):
            d = tt - j['t']
            ok = np.flatnonzero((d >= -0.15) & (d <= 0.6))
            return int(ok[0]) if len(ok) else None
        rows = []
        for j in judged:
            m = phys(j)
            rows.append(dict(t=round(j['t'], 3), result=j['result'], expected_mm=j['expected'], pol=j['pol'], pwm=j['pwm'],
                             level=round(j['level'], 1), magnet_t_peak=None if m is None else round(tt[m], 3),
                             magnet_pol=None if m is None else tru[m][1]))
        detected = {phys(j) for j in judged} - {None}
        rec = dict(label=rb.LABEL27[boot], truth_magnets=len(tru), raw_onsets_under_gate=n_raw, judged=len(judged),
                   confirmed=len(conf), nonqualifying=len(judged) - len(conf),
                   magnets_with_a_judged_onset=len(detected),
                   gate_open_s=round(sum(b - a for a, b in spans), 1), gate_spans=[(round(a, 2), round(b, 2)) for a, b in spans][:40],
                   rows=rows)
        focus = {}
        for (b, mm), tp in r7.FOCUS.items():
            if b == boot:
                inside = any(a <= tp <= c for a, c in spans)
                focus[mm] = dict(gate_open_at_peak=inside,
                                 judged_within_1_5s=[r for r in rows if abs(r['t'] - tp) < 1.5])
        rec['focus'] = focus
        res[boot] = rec
        print(boot, rec['label'], {k: rec[k] for k in ('truth_magnets', 'raw_onsets_under_gate', 'judged', 'confirmed',
                                                        'nonqualifying', 'magnets_with_a_judged_onset', 'gate_open_s')})
        for r in rows[:8]:
            print('   ', r)
        print('    gate', rec['gate_spans'][:6], 'focus', json.dumps(focus))
    if len(sys.argv) > 3:
        Path(sys.argv[3]).write_text(json.dumps(res, indent=1))


if __name__ == '__main__':
    main()
