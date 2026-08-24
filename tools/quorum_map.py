#!/usr/bin/env python3
"""quorum_map.py — extracts QUORUM's authoritative track map, polarity
sequence, and navigation constants directly from firmware/QUORUM/QUORUM.ino,
so the host-replay prototype (tools/hwt_gate_replay.py) never hand-duplicates
data that already has one source of truth in the firmware.

INVESTIGATORY. Read-only: parses QUORUM.ino as text with regular
expressions. Never compiles, executes, or writes to the firmware. If a
future edit renames or reshapes one of the extracted symbols, this raises
rather than silently returning stale or wrong data.

    python3 -c "from quorum_map import QuorumMap; m = QuorumMap(); print(m.n, m.dna[:10])"
"""
import re

DEFAULT_QUORUM_INO = "firmware/QUORUM/QUORUM.ino"


def _extract_int_array(text, name):
    m = re.search(r'\b%s\s*\[[^\]]*\]\s*(?:PROGMEM)?\s*=\s*\{(.*?)\};' % re.escape(name),
                 text, re.S)
    if not m:
        raise ValueError("could not find array %r in QUORUM.ino" % name)
    return [int(n) for n in re.findall(r'-?\d+', m.group(1))]


def _extract_define(text, name):
    """Matches either a #define or a `static const <type> NAME = value;`."""
    m = re.search(r'#define\s+%s\s+([0-9.eE+-]+)' % re.escape(name), text)
    if not m:
        m = re.search(r'\b%s\s*=\s*([0-9.eE+-]+)f?\s*;' % re.escape(name), text)
    if not m:
        raise ValueError("could not find constant %r in QUORUM.ino" % name)
    return float(m.group(1))


class QuorumMap:
    """The track map and navigation constants, read straight from the
    firmware that is the one source of truth for them.

    dna[mm]          polarity at marker mm: 1=N, 0=S (matches QUORUM's
                     dnaAt()/polChar() convention exactly)
    spacing_mm[mm]   physical distance, mm, from marker mm to marker mm+1
                     when travelling CW (matches QUORUM's spacingMm[]
                     indexing -- see spacing_between() for the CCW case,
                     which QUORUM computes via the *next* marker's slot)
    """

    def __init__(self, path=DEFAULT_QUORUM_INO):
        with open(path) as fh:
            text = fh.read()
        self.source_path = path

        self.dna = _extract_int_array(text, "NGR_DNA1")
        self.spacing_mm = _extract_int_array(text, "spacingMm")
        if len(self.dna) != len(self.spacing_mm):
            raise ValueError("DNA length %d != spacing length %d -- QUORUM_map "
                             "extraction found mismatched arrays" %
                             (len(self.dna), len(self.spacing_mm)))
        self.n = len(self.dna)

        # Detector (Layer 2)
        self.event_floor_ms     = _extract_define(text, "EVENT_FLOOR_MS")
        self.event_exit_hold_ms = _extract_define(text, "EVENT_EXIT_HOLD_MS")

        # Timing gate (§3) and QUORUM evaluation (§2)
        self.gate_low_pwm_floor = _extract_define(text, "GATE_LOW_PWM_FLOOR")
        self.gate_ramp_delta    = _extract_define(text, "GATE_RAMP_DELTA")
        self.dt_conserve_tol    = _extract_define(text, "DT_CONSERVE_TOL")
        self.vel_model_slope     = _extract_define(text, "VEL_MODEL_SLOPE")
        self.vel_model_intercept = _extract_define(text, "VEL_MODEL_INTERCEPT")
        self.quorum_trigger = int(_extract_define(text, "QUORUM_TRIGGER"))
        self.quorum_max     = int(_extract_define(text, "QUORUM_MAX"))
        self.quorum_margin  = int(_extract_define(text, "QUORUM_MARGIN"))

    @classmethod
    def from_data(cls, dna, spacing_mm, *, vel_model_slope=3.90, vel_model_intercept=-99.2,
                 event_floor_ms=40.0, event_exit_hold_ms=20.0, gate_low_pwm_floor=40.0,
                 gate_ramp_delta=10.0, dt_conserve_tol=0.30, quorum_trigger=3,
                 quorum_max=12, quorum_margin=2):
        """Build a QuorumMap from in-memory arrays/constants instead of
        parsing QUORUM.ino -- for host tests that need a small, fixed,
        deterministic map independent of the real firmware's current
        contents. Defaults mirror QUORUM.ino's own values at the time this
        was written; tests should not rely on them staying in sync with the
        firmware -- pass explicit values for anything the test depends on."""
        self = cls.__new__(cls)
        self.source_path = "<in-memory>"
        self.dna = list(dna)
        self.spacing_mm = list(spacing_mm)
        if len(self.dna) != len(self.spacing_mm):
            raise ValueError("DNA length %d != spacing length %d" %
                             (len(self.dna), len(self.spacing_mm)))
        self.n = len(self.dna)
        self.event_floor_ms = event_floor_ms
        self.event_exit_hold_ms = event_exit_hold_ms
        self.gate_low_pwm_floor = gate_low_pwm_floor
        self.gate_ramp_delta = gate_ramp_delta
        self.dt_conserve_tol = dt_conserve_tol
        self.vel_model_slope = vel_model_slope
        self.vel_model_intercept = vel_model_intercept
        self.quorum_trigger = quorum_trigger
        self.quorum_max = quorum_max
        self.quorum_margin = quorum_margin
        return self

    # -- map helpers, mirroring QUORUM's own routeMod()/dnaAt()/nextMm() ----
    def route_mod(self, v):
        return v % self.n

    def dna_at(self, mm):
        return self.dna[self.route_mod(mm)]

    def next_mm(self, mm, direction):
        """direction: +1 for CW, -1 for CCW (matches QUORUM's MAP_CW/MAP_CCW)."""
        return self.route_mod(mm + direction)

    def spacing_between(self, mm, direction):
        """Distance from marker mm to the NEXT marker in `direction`. Mirrors
        navOnMarker()'s conserveIntervalIndex: CW reads spacing_mm[mm] (the
        interval leaving mm); CCW reads spacing_mm[the marker being entered],
        because spacing_mm[i] is defined as "i to i+1", which IS "entered
        marker to mm" when travelling CCW."""
        if direction > 0:
            return self.spacing_mm[self.route_mod(mm)]
        return self.spacing_mm[self.route_mod(mm + direction)]

    def velocity_mm_s(self, pwm_actual):
        """QUORUM's own provisional PWM->velocity model (§3). The sketch's
        own comment calls this "explicitly provisional: PWM is a request,
        not a result" -- inherited here for the same reason, not
        independently re-validated against wheel-speed ground truth, which
        none of these captures have."""
        return self.vel_model_slope * pwm_actual + self.vel_model_intercept

    def max_credible_speed_mm_s(self):
        """QUORUM's own velocity model evaluated at full throttle (PWM 255)
        -- an inherited upper envelope on how fast the locomotive could
        plausibly be moving, used only to derive a MINIMUM possible time to
        the next marker (see hwt_gate_replay.py's physical-timing gate).
        This is explicitly a re-use of QUORUM's own provisional figure, not
        a new, independently-measured bound."""
        return self.velocity_mm_s(255.0)
