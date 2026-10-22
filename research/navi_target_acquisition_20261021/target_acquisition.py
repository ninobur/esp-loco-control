"""Causal, non-IR NAVI target-acquisition recognizer.

Implements the architecture fixed in
docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md:

    CLOSE -> TARGET -> BASELINE -> ACQUIRE -> CLOSE AND ADVANCE

exactly, and only, as that document (and the assignment that requested
this implementation) specify. In particular, by direction:

  - TARGET is the deterministic next mapped magnet (route_map.next_marker),
    never a hypothesis among several.
  - ACQUIRE has no automatic distance- or time-based exit. A candidate
    that contradicts the target's expected polarity is disposed of --
    its evidence preserved and reported -- without advancing, without
    inventing a different identity for it, and without abandoning the
    target. There is no LOST transition, no elapsed-time timeout, and no
    maximum rejected-observation count anywhere in this module.
  - PWM/elapsed time are recorded for telemetry only. They never gate an
    accept/reject/dispose decision (see `Observation.pwm` below).
  - Motive PWM = 0 pauses the recognizer (decisions 0106/0107): a
    stationary sample is retained for the trace but contributes nothing
    to baseline fitting, candidate duration, or return-to-baseline
    detection, and does not discard whatever candidate was open.

The line-versus-Gaussian shape evidence is adapted from
research/navi_wolf_pack/normal_curve_recognizer.py (commit 98db802); it
is used here as supportive waveform evidence recorded alongside the
target-polarity/departure decision, not as an additional independent gate
layered on top of it (the assignment's instruction: "do not add
independent thresholds simply because a measurement is available").
"""
from __future__ import annotations

from dataclasses import dataclass, field
import math
import numpy as np

import route_map

# ---------------------------------------------------------------------------
# Thresholds, each with a stated justification. None is derived from IR or
# the historical +-15% tolerance; both are out of scope for this experiment.
# ---------------------------------------------------------------------------

# Hall departure criterion: given verbatim by the assignment ("at least 70
# ADC counts... for two consecutive samples"). Applied to 20-ms bin means,
# the same granularity the rest of this pipeline and the prior research
# (98db802) already use.
HALL_DEPARTURE_ADC = 70.0
DEPARTURE_CONSECUTIVE_BINS = 2

# Minimum quiet span before a baseline fit is trusted: decision 0093's
# existing 650-ms Hall-only fallback guard, chosen here because it is
# already-established Hall-only practice (not IR-derived) and is
# conservative relative to the corrected width study's own measured
# preceding-baseline minimum of 0.72 s (a15036f) -- i.e. it asks for less
# than what genuine baselines were actually observed to provide.
MIN_BASELINE_SPAN_S = 0.65
MIN_BASELINE_BINS = 8  # mirrors 98db802's own minimum sample count.

# Candidate minimum sustained duration before a matching candidate is
# provisionally ACQUIRED: the prior research's own empirical starting
# point for PWM 90 (a15036f / 98db802's 110 ms gate). The assignment asks
# that this be evaluated, not assumed; §"Timing" in the report does so.
MIN_ACQUIRE_DURATION_S = 0.11

# Return-to-baseline: 120 ms of renewed baseline-consistent samples (6
# consecutive 20-ms bins), unchanged from 98db802 -- RTB is recognized
# from the phenomenon (0107), not asserted by a fixed total duration.
RTB_QUIET_BINS = 6
RTB_QUIET_SIGMA = 3.0

# The causal line-vs-Gaussian fit is recomputed from scratch on every new
# sample (98db802's own method, unchanged). Bounding it to the most recent
# window keeps that O(n) per-sample cost from growing with how long a
# candidate has been open -- generous relative to every measured curve
# duration in the corrected width study (max 1.67 s; this is 3 s) and
# does not change which samples decide ACQUIRE/RTB, only the shape-fit
# window used for the supportive ΔBIC trace.
BIC_FIT_WINDOW_BINS = 150

# A candidate's resolved sign must be based on the *fitted baseline*, not
# a hard global zero -- consistent with 0107 ("the Hall baseline is
# spatial information").


def hall_sign_for_polarity(polarity: int) -> int:
    """Otto convention (RvmNavigator.h, commit 3a8953c): North (1) = ADC
    departs above baseline; South (0) = ADC departs below baseline."""
    return 1 if polarity else -1


def _bic(sse: float, n: int, p: int) -> float:
    return n * math.log(max(sse / n, 1e-9)) + p * math.log(n)


def delta_bic(t: np.ndarray, y: np.ndarray) -> float:
    """BIC(line) - BIC(line + one Gaussian). Adapted, unchanged in method,
    from 98db802's own causal bounded grid fit. Supportive waveform
    evidence only -- see module docstring."""
    if len(y) < 9:
        return float("-inf")
    x = (t - t[0]) / max(t[-1] - t[0], 1e-9)
    line_x = np.column_stack((np.ones(len(x)), x))
    line_sse = float(np.sum((y - line_x @ np.linalg.lstsq(line_x, y, rcond=None)[0]) ** 2))
    best = float("inf")
    for mu in np.linspace(.18, .82, 9):
        for sigma in (.08, .12, .18, .25, .35, .50):
            g = np.exp(-.5 * ((x - mu) / sigma) ** 2)
            design = np.column_stack((np.ones(len(x)), x, g))
            residual = y - design @ np.linalg.lstsq(design, y, rcond=None)[0]
            best = min(best, float(np.sum(residual ** 2)))
    return _bic(line_sse, len(y), 2) - _bic(best, len(y), 5)


@dataclass
class Candidate:
    start_s: float
    samples: list = field(default_factory=list)   # (t, y) pairs, movement-time only
    resolved_sign: int | None = None
    matches_target: bool | None = None
    target_mm: int = -1
    target_polarity: int = -1
    acquired_s: float | None = None    # provisional ACQUIRE declaration
    peak_bic: float = float("-inf")
    peak_departure: float = 0.0
    quiet_run: int = 0
    last_active_t: float = 0.0   # last sample still actively departing (not RTB-quiet)
    closed_s: float = None


@dataclass
class Event:
    kind: str           # OPEN_CANDIDATE | ACQUIRED | CONTRADICTION | CLOSE_AND_ADVANCE | DISPOSED
    t: float
    target_mm: int
    detail: dict


@dataclass
class TargetAcquisition:
    """One continuous causal run of CLOSE -> TARGET -> BASELINE -> ACQUIRE.

    `last_confirmed_mm` and `direction` are the declared starting reference
    -- this experiment never discovers them; see replay.py and the report's
    discussion of what "trusted geographic position" means for a recording
    with no independent surveyed ground truth.
    """
    last_confirmed_mm: int
    direction: int
    baseline_hist: list = field(default_factory=list)   # (t, y) quiet samples
    baseline_fit: tuple | None = None   # (level, slope, noise) at last fit time t0
    baseline_fit_t0: float = 0.0
    candidate: Candidate | None = None
    events: list = field(default_factory=list)
    target_mm: int = -1
    _paused_pending: bool = False
    # Overridable for the explicit, data-driven comparison in REPORT.md
    # ("Timing"/"what remains unvalidated") -- not a second independent
    # threshold, the same one the assignment names as an evaluable
    # starting point.
    min_acquire_duration_s: float = MIN_ACQUIRE_DURATION_S

    def __post_init__(self):
        self.target_mm = route_map.next_marker(self.last_confirmed_mm, self.direction)

    @property
    def target_polarity(self) -> int:
        return route_map.polarity_at(self.target_mm)

    def _try_establish_baseline(self):
        """BASELINE step: fit once from a sufficiently long quiet stretch,
        then FREEZE it. The fit is not continuously re-extended sample by
        sample while ACQUIRE is watching -- doing so lets a slow ramp (the
        leading edge of a real magnetic curve) get absorbed into the
        baseline itself before it ever crosses the departure criterion,
        which is exactly what "do not extrapolate a fitted baseline
        through an active magnetic curve" (0107) rules out. The baseline
        is re-established fresh only after the next encounter closes.
        """
        if self.baseline_fit is not None:
            return
        if len(self.baseline_hist) < MIN_BASELINE_BINS:
            return
        span = self.baseline_hist[-1][0] - self.baseline_hist[0][0]
        if span < MIN_BASELINE_SPAN_S:
            return
        a = np.asarray(self.baseline_hist)
        t0 = a[-1, 0]
        coeff = np.polyfit(a[:, 0] - t0, a[:, 1], 1)
        residual = a[:, 1] - np.polyval(coeff, a[:, 0] - t0)
        self.baseline_fit = (float(coeff[1]), float(coeff[0]), max(float(np.std(residual)), 1.0))
        self.baseline_fit_t0 = t0

    def _expected(self, t: float) -> tuple[float, float] | None:
        if self.baseline_fit is None:
            return None
        level, slope, noise = self.baseline_fit
        return level + slope * (t - self.baseline_fit_t0), noise

    def observe(self, t: float, hall: float | None, pwm: int) -> list:
        """Consume one 20-ms bin. `hall` is None for a missing bin (a
        genuine sensor gap -- missing evidence, 0108 -- not a stop)."""
        if pwm == 0:
            # Motive PWM = 0: non-navigation interval (0106/0107). Retained
            # for the trace by the caller; contributes nothing here, and
            # whatever candidate/baseline state existed is left untouched.
            return []
        if hall is None or not math.isfinite(hall):
            # A missing bin censors evidence; it never bridges an encounter
            # and never discards one either -- just contributes nothing.
            return []
        if self.candidate is None:
            return self._observe_baseline(t, hall)
        return self._observe_candidate(t, hall)

    def _observe_baseline(self, t: float, hall: float) -> list:
        self._try_establish_baseline()
        expected = self._expected(t)
        if expected is None:
            # Baseline not yet established: keep collecting the quiet
            # stretch BASELINE needs. No departure test is meaningful
            # without an established reference.
            self.baseline_hist.append((t, hall))
            return []
        level, noise = expected
        departure = hall - level
        self._pending_departures = getattr(self, "_pending_departures", [])
        if abs(departure) >= HALL_DEPARTURE_ADC:
            sign = 1 if departure > 0 else -1
            if self._pending_departures and self._pending_departures[-1][1] != sign:
                self._pending_departures = []
            self._pending_departures.append((t, sign, hall, departure))
            if len(self._pending_departures) >= DEPARTURE_CONSECUTIVE_BINS:
                start_t, sign, _, _ = self._pending_departures[0]
                self.candidate = Candidate(
                    start_s=start_t, resolved_sign=sign,
                    target_mm=self.target_mm, target_polarity=self.target_polarity,
                    last_active_t=t,
                )
                self.candidate.matches_target = (sign == hall_sign_for_polarity(self.target_polarity))
                self.candidate.samples = [(tt, yy) for tt, _, yy, _ in self._pending_departures]
                self.candidate.peak_departure = max(abs(d) for _, _, _, d in self._pending_departures)
                self._pending_departures = []
                self.events.append(Event("OPEN_CANDIDATE", t, self.target_mm, dict(
                    sign=sign, matches_target=self.candidate.matches_target,
                )))
        else:
            # A frozen baseline does not re-absorb samples -- it is only
            # ever re-established after an encounter closes (see
            # observe_candidate's close handling).
            self._pending_departures = []
        return []

    def _observe_candidate(self, t: float, hall: float) -> list:
        c = self.candidate
        completed: list = []
        c.samples.append((t, hall))
        window = c.samples[-BIC_FIT_WINDOW_BINS:]
        times = np.asarray([s[0] for s in window])
        values = np.asarray([s[1] for s in window])
        bic = delta_bic(times, values)
        c.peak_bic = max(c.peak_bic, bic)

        # Return-to-baseline: compared against the FROZEN pre-candidate
        # baseline fit, never one extrapolated through the candidate
        # itself (0107). This also determines whether the sample is still
        # "actively departing" for duration purposes below.
        level, noise = self._expected(c.start_s) or (hall, 1.0)
        departure = abs(hall - level)
        c.peak_departure = max(c.peak_departure, departure)
        still_active = departure > max(HALL_DEPARTURE_ADC / 2.0, RTB_QUIET_SIGMA * noise)
        if still_active:
            c.last_active_t = t
            c.quiet_run = 0
        else:
            c.quiet_run += 1

        # Sustained duration is measured over the active encounter only --
        # a quiet tail accumulating toward RTB must not inflate it.
        active_duration = c.last_active_t - c.start_s
        if c.matches_target and c.acquired_s is None and active_duration >= self.min_acquire_duration_s:
            c.acquired_s = c.last_active_t
            self.events.append(Event("ACQUIRED", c.last_active_t, self.target_mm, dict(
                start_s=c.start_s, peak_bic=c.peak_bic, duration_s=active_duration,
            )))

        if c.quiet_run >= RTB_QUIET_BINS:
            close_t = t - (RTB_QUIET_BINS - 1) * 0.02
            c.closed_s = close_t
            if c.matches_target and c.acquired_s is not None:
                self.events.append(Event("CLOSE_AND_ADVANCE", close_t, self.target_mm, dict(
                    start_s=c.start_s, acquired_s=c.acquired_s, peak_bic=c.peak_bic,
                    duration_s=close_t - c.start_s,
                )))
                self.last_confirmed_mm = self.target_mm
                self.target_mm = route_map.next_marker(self.last_confirmed_mm, self.direction)
                completed.append(c)
            else:
                kind = "CONTRADICTION" if not c.matches_target else "DISPOSED"
                self.events.append(Event(kind, close_t, self.target_mm, dict(
                    start_s=c.start_s, resolved_sign=c.resolved_sign,
                    peak_bic=c.peak_bic, peak_departure=c.peak_departure,
                    duration_s=close_t - c.start_s, acquired=c.acquired_s is not None,
                )))
            # The quiet samples that closed the candidate seed the next
            # BASELINE phase; nothing about the target changes on dispose.
            self.baseline_hist = c.samples[-RTB_QUIET_BINS:]
            self.baseline_fit = None
            self.candidate = None
        return completed
