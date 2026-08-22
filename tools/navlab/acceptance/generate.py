"""Deterministic generated event streams with explicit truth at every event.

Every stream is built on the real committed map, from a named integer seed, and
carries the true (marker, direction) for every detection plus the true elapsed
time. Nothing here consumes a firmware label, a firmware verdict or an MQTT
receipt time; the decoy fields exist only so the harness can prove the
navigator ignores them.
"""
import random
from dataclasses import dataclass, field

from . import ngrmap as M
from .navapi import Detection

# Empirical-ish signatures, from the committed classification work. Used only
# to SHAPE generated events; no test verdict depends on these numbers.
GENUINE_PEAK = (110, 260)
GENUINE_DUR = (120, 380)
GHOST_PEAK = (30, 60)
GHOST_DUR = (20, 70)

# Nominal speed per PWM bucket, mm/ms. Generator physics only.
SPEED = {0: 0.0, 40: 0.095, 60: 0.130, 75: 0.180, 90: 0.240, 99: 0.300, 100: 0.305}


def pol(mm):
    return 'N' if M.DNA[mm] == 1 else 'S'


@dataclass
class TruthEvent:
    """Ground truth for one emitted detection. The harness's only oracle."""
    index: int
    detection: Detection
    true_mm: int                 # marker the locomotive is at, if genuine
    true_dir: int
    is_genuine: bool             # False => ghost, no marker was crossed
    true_elapsed_ms: int         # since the previous GENUINE crossing
    missed_before: int = 0       # genuine markers crossed but not detected
    note: str = ''


@dataclass
class Stream:
    name: str
    seed: int
    events: list = field(default_factory=list)
    start_mm: int = None
    start_dir: int = None
    clean: bool = True           # CLEAN families forbid a stop; AMBIGUOUS allow
    reversals: list = field(default_factory=list)
    declarations: list = field(default_factory=list)
    peer_reports: list = field(default_factory=list)
    notes: str = ''
    start_mode: str = 'exact'          # navapi.MODE_*
    start_mm_declared: int = None      # what the OPERATOR declared, if anything
    externally_authorised: bool = True # movement supplied from outside the navigator

    def truth_at(self, i):
        return self.events[i]

    def final_truth(self):
        for e in reversed(self.events):
            if e.is_genuine:
                return e.true_mm, e.true_dir
        return self.start_mm, self.start_dir


class Generator:
    """Deterministic. Same seed => byte-identical stream."""

    def __init__(self, seed):
        self.seed = seed
        self.rng = random.Random(seed)
        self.t = 1000
        self.epoch = 1

    # -- primitives ----------------------------------------------------------
    def _advance_time(self, mm, step, pwm):
        v = SPEED.get(pwm, 0.13)
        d = M.step_mm(mm, step)
        jitter = self.rng.uniform(0.92, 1.08)
        return max(1, int(round(d / v * jitter)))

    def _genuine(self, mm, elapsed, pwm):
        return Detection(
            t_detect=self.t, clock_epoch=self.epoch, polarity=pol(mm),
            peak=self.rng.randint(*GENUINE_PEAK),
            duration_ms=self.rng.randint(*GENUINE_DUR),
            pwm_actual_history=[(elapsed, pwm), (0, pwm)],
            decoy_firmware_mm=self.rng.randrange(M.DNA_N),
            decoy_firmware_verdict=self.rng.choice(['ACCEPTED', 'QUARANTINED']),
            decoy_mqtt_recv_ts=self.t + self.rng.randint(200, 3000))

    def _ghost(self, polarity, pwm, elapsed):
        return Detection(
            t_detect=self.t, clock_epoch=self.epoch, polarity=polarity,
            peak=self.rng.randint(*GHOST_PEAK),
            duration_ms=self.rng.randint(*GHOST_DUR),
            pwm_actual_history=[(elapsed, pwm), (0, pwm)],
            decoy_firmware_mm=self.rng.randrange(M.DNA_N),
            decoy_firmware_verdict='QUARANTINED',
            decoy_mqtt_recv_ts=self.t + self.rng.randint(200, 3000))

    # -- streams -------------------------------------------------------------
    def clean_run(self, start_mm, step, n, pwm=60, name='clean'):
        """n genuine markers, nothing else. The CLEAN baseline."""
        s = Stream(name=name, seed=self.seed, start_mm=start_mm, start_dir=step)
        mm = start_mm
        for i in range(n):
            el = self._advance_time(mm, step, pwm)
            self.t += el
            mm = M.nxt(mm, step)
            s.events.append(TruthEvent(i, self._genuine(mm, el, pwm),
                                       mm, step, True, el))
        return s

    def with_ghosts(self, start_mm, step, n, ghost_at, pwm=60, name='ghosts',
                    repeat=1):
        """Insert `repeat` ghost detections before the genuine event at each
        index in ghost_at. Ghosts cross no marker: truth does not advance."""
        s = Stream(name=name, seed=self.seed, start_mm=start_mm, start_dir=step,
                   clean=(repeat == 1))
        mm, idx = start_mm, 0
        for i in range(n):
            if i in ghost_at:
                for _ in range(repeat):
                    self.t += self.rng.randint(60, 140)
                    gp = 'N' if self.rng.random() < 0.5 else 'S'
                    s.events.append(TruthEvent(idx, self._ghost(gp, pwm, 100),
                                               mm, step, False, 0,
                                               note='ghost'))
                    idx += 1
            el = self._advance_time(mm, step, pwm)
            self.t += el
            mm = M.nxt(mm, step)
            s.events.append(TruthEvent(idx, self._genuine(mm, el, pwm),
                                       mm, step, True, el))
            idx += 1
        return s

    def with_missed(self, start_mm, step, n, miss_at, miss_len, pwm=60,
                    name='missed'):
        """miss_len genuine crossings produce no detection starting at miss_at."""
        s = Stream(name=name, seed=self.seed, start_mm=start_mm, start_dir=step,
                   clean=(miss_len <= 1))
        mm, idx, i = start_mm, 0, 0
        while i < n:
            if i == miss_at:
                skipped, el_total = 0, 0
                for _ in range(miss_len):
                    el_total += self._advance_time(mm, step, pwm)
                    mm = M.nxt(mm, step)
                    skipped += 1
                    i += 1
                self.t += el_total
                el = self._advance_time(mm, step, pwm)
                self.t += el
                mm = M.nxt(mm, step)
                s.events.append(TruthEvent(idx, self._genuine(mm, el_total + el, pwm),
                                           mm, step, True, el_total + el,
                                           missed_before=skipped,
                                           note='after %d missed' % skipped))
                idx += 1
                i += 1
                continue
            el = self._advance_time(mm, step, pwm)
            self.t += el
            mm = M.nxt(mm, step)
            s.events.append(TruthEvent(idx, self._genuine(mm, el, pwm),
                                       mm, step, True, el))
            idx += 1
            i += 1
        return s

    def with_reversal(self, start_mm, step, n_before, n_after, pwm=60,
                      name='reversal'):
        s = self.clean_run(start_mm, step, n_before, pwm, name)
        mm = s.events[-1].true_mm if s.events else start_mm
        rev = -step
        s.reversals.append(len(s.events))
        for i in range(n_after):
            el = self._advance_time(mm, rev, pwm)
            self.t += el
            mm = M.nxt(mm, rev)
            s.events.append(TruthEvent(len(s.events), self._genuine(mm, el, pwm),
                                       mm, rev, True, el, note='post-reversal'))
        return s

    def with_reread(self, start_mm, step, n_reread, pwm=0, name='reread'):
        """Stationary re-reads of ONE magnet: same marker, same polarity."""
        s = Stream(name=name, seed=self.seed, start_mm=start_mm, start_dir=step,
                   clean=False)
        mm = start_mm
        for i in range(n_reread):
            self.t += self.rng.randint(300, 900)
            s.events.append(TruthEvent(i, self._genuine(mm, 400, pwm),
                                       mm, step, True, 400,
                                       note='same-magnet reread'))
        return s

    def with_acceleration(self, start_mm, step, n, lo=40, hi=90, name='accel'):
        s = Stream(name=name, seed=self.seed, start_mm=start_mm, start_dir=step)
        mm = start_mm
        for i in range(n):
            pwm = lo if i < n // 2 else hi
            el = self._advance_time(mm, step, pwm)
            self.t += el
            mm = M.nxt(mm, step)
            s.events.append(TruthEvent(i, self._genuine(mm, el, pwm),
                                       mm, step, True, el,
                                       note='accelerating' if i == n // 2 else ''))
        return s

    def with_discontinuity(self, start_mm, step, n_before, gap_intervals,
                           n_after, pwm=60, name='discontinuity'):
        """Case I: the monotonic clock epoch changes, and `gap_intervals`
        genuine markers really were crossed during the unknown time."""
        s = self.clean_run(start_mm, step, n_before, pwm, name)
        mm = s.events[-1].true_mm if s.events else start_mm
        for _ in range(gap_intervals):
            mm = M.nxt(mm, step)
        self.epoch += 1
        self.t = self.rng.randint(10, 400)
        s.notes = 'clock epoch %d, true gap %d intervals' % (self.epoch, gap_intervals)
        for i in range(n_after):
            el = self._advance_time(mm, step, pwm)
            self.t += el
            mm = M.nxt(mm, step)
            s.events.append(TruthEvent(len(s.events), self._genuine(mm, el, pwm),
                                       mm, step, True, el,
                                       note='post-discontinuity'))
        return s

    def with_redeclaration(self, start_mm, step, n, at, pwm=60,
                           name='redeclaration'):
        """Case R: an internal firmware re-anchor. The monotonic clock is
        UNAFFECTED, so elapsed stays known -- spec 3.7. The only thing the
        redeclaration carries is a label, which is not evidence."""
        s = self.clean_run(start_mm, step, n, pwm, name)
        if 0 <= at < len(s.events):
            s.events[at].note = 'internal redeclaration (label only)'
            s.events[at].detection.decoy_firmware_mm = (
                s.events[at].true_mm + 37) % M.DNA_N
            s.events[at].detection.decoy_firmware_verdict = 'REDECLARED'
        return s
