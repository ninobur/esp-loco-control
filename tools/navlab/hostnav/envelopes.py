"""Robust timing-envelope interface -- specification 3.8, principle P9.

The navigator never consumes a single minimum, a dwell-contaminated sample or
a ghost-contaminated sample as a bound. It consumes an *envelope object* with
one question: given a PWM profile over an elapsed interval, what distance
window `[d_lo, d_hi]` could the locomotive have covered?

Two implementations live here:

* `NominalEnvelope` -- the default. It bounds the speed profile by the
  extremes of the PWM actually recorded over the interval and applies the
  `SPEED_BAND_LO`/`SPEED_BAND_HI` band of section 10. Both ends are genuine
  bounds by construction, which is what the completeness invariant needs.
  Its speed table is **uncalibrated** (see `params`).
* `TableEnvelope` -- the shape the versioned, calibrated table of 3.8 will
  take when it exists: quantile fast bounds per (section, direction,
  locomotive) tier with `MIN_N` fallback and the `SANITY_RATIO` rejection.
  It is wired in but has no committed table to load, so the navigator uses
  the nominal envelope and says so.

Nothing here consumes a firmware label, a firmware verdict or an MQTT receipt
time (P4).
"""
from . import params as P


class PwmHistory:
    """A short ring of (t, pwm) samples on the detection clock.

    Elapsed time is branch-local (3.6), so a bound may be asked for a window
    that started several detections ago. The ring is what makes that possible
    without precomputing anything per event.
    """

    def __init__(self, capacity=64):
        self._samples = []
        self._capacity = capacity

    def clear(self):
        self._samples = []

    def add_detection(self, t_detect, pwm_actual_history):
        """Fold one detection's own PWM profile into the ring."""
        for offset, pwm in (pwm_actual_history or ()):
            self._samples.append((t_detect - offset, int(pwm)))
        if not pwm_actual_history:
            self._samples.append((t_detect, 0))
        self._samples.sort(key=lambda s: s[0])
        if len(self._samples) > self._capacity:
            del self._samples[:len(self._samples) - self._capacity]

    def pwm_range(self, t_from, t_to):
        """(min pwm, max pwm) in force over [t_from, t_to]."""
        vals = [pwm for t, pwm in self._samples if t_from <= t <= t_to]
        before = [pwm for t, pwm in self._samples if t < t_from]
        if before:
            vals.append(before[-1])
        if not vals:
            vals = [pwm for _, pwm in self._samples[-1:]] or [0]
        return min(vals), max(vals)


class NominalEnvelope:
    """Uncalibrated nominal envelope. See params.NOMINAL_SPEED_MM_PER_MS."""

    calibrated = False
    name = 'nominal-uncalibrated'

    def distance_window(self, history, t_from, t_to):
        """Genuine `[d_lo, d_hi]` for the interval, in mm."""
        elapsed = max(0, t_to - t_from)
        _, hi_pwm = history.pwm_range(t_from, t_to)
        v_hi = P.nominal_speed(hi_pwm) * P.SPEED_BAND_HI
        return P.SPEED_BAND_LO * elapsed, v_hi * elapsed

    def stopped(self, history, t_from, t_to):
        """True when standstill over the interval cannot be excluded."""
        _, hi_pwm = history.pwm_range(t_from, t_to)
        return P.nominal_speed(hi_pwm) <= 0.0


class TableEnvelope(NominalEnvelope):
    """Versioned quantile envelope table, 3.8. No committed table exists.

    Kept as the seam the calibrated table plugs into: tiers are consulted
    loosest-adequately-populated first, a tier with fewer than `MIN_N`
    admitted samples produces no bound, and a bound implying a corridor speed
    above `SANITY_RATIO` x the bucket median is rejected as contaminated.
    Until a table is committed it degrades to the nominal envelope, and
    reports that it did.
    """

    calibrated = False
    name = 'table-envelope (no committed table; degraded to nominal)'

    def __init__(self, table=None):
        self.table = table or {}

    def _tier_bound(self, key):
        row = self.table.get(key)
        if not row or row.get('n', 0) < P.MIN_N:
            return None
        if row.get('implied_speed_ratio', 0.0) > P.SANITY_RATIO:
            return None
        return row.get('fast_dt')
