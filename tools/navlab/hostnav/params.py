"""Engineering parameters -- specification section 10.

**None of these values carries calibration evidence.** They are the
specification's recommended candidate defaults, or the smallest honest
placeholder where the specification gives none. Specification 10 blocks
candidate freeze until each is justified by committed calibration evidence,
and acceptance result `N2` stays NOT_DEMONSTRATED for exactly that reason.
Nothing here may be described as calibrated, tuned or field-validated.

These are engineering parameters, not operator decisions. Operator decisions
live in `navapi.Policy`; operator rulings already closed are not configurable
anywhere.
"""

# --- 3.8 timing envelopes ---------------------------------------------------
#: Robust-quantile envelope generation parameters. Recorded because the
#: specification names them; envelope generation itself stays off-locomotive
#: and the host model consumes the band below.
Q_FAST = 0.02
MARGIN = 0.15
SANITY_RATIO = 3.0
MIN_N = 8

#: Nominal PWM -> speed characterisation, mm/ms. **Uncalibrated.** This is a
#: stand-in for the versioned envelope table of 3.8, which does not exist yet.
#: Every acceptance conclusion that depends on a distance window depends on
#: this table, and that dependency is recorded rather than hidden.
NOMINAL_SPEED_MM_PER_MS = {
    0: 0.000, 40: 0.095, 60: 0.130, 75: 0.180, 90: 0.240, 99: 0.300,
    100: 0.305,
}

#: Upper band applied to the nominal speed. `d_hi` must never fall below the
#: true distance or the completeness invariant (P5/S2) is lost, so it is
#: deliberately loose.
SPEED_BAND_HI = 1.30

#: The lower distance bound is **zero**, and that is not laziness. Nothing in
#: the evidence record of 3.2 excludes the locomotive having been slower than
#: nominal at any point inside an interval the navigator only samples at its
#: ends, so any positive `d_lo` derived from nominal speed is an
#: under-approximation and can exclude the truth. Standstill is handled
#: separately and correctly: the same-marker candidate is admitted only when
#: the PWM profile leaves standstill possible (see `envelopes.stopped`), which
#: is what keeps a moving locomotive from accumulating stall hypotheses
#: without ever excluding a real slow interval.
SPEED_BAND_LO = 0.0

#: Within this fraction of a timing bound a detection forks rather than being
#: vetoed (4.1), so genuine acceleration is retained.
MARGINAL_SLACK = 0.25

# --- 3.9 amplitude / duration priors ---------------------------------------
#: Classification inputs only. They suppress no detection and delete nothing;
#: a GHOST_LIKE reading forks a pending branch (P8).
PEAK_FLOOR = 80
DUR_FLOOR = 100

# --- 3.10 pending branches --------------------------------------------------
PENDING_DEPTH_MAX = 3
#: The branch list is the design's only capped structure (3.5). Overflow
#: collapses to the union and costs confirmation authority, never hypotheses.
BRANCH_MAX = 8

# --- 3.11 confirmation ------------------------------------------------------
K_CONFIRM = 3
COLLAPSE_MAX_SET = 8

# --- 3.12 occupancy and fleet safety ---------------------------------------
OCC_ARCS_MAX = 3
ROUTE_WIDE_FRACTION = 0.6
#: Decision 0033 bubble, in markers, and the conservative train extent used by
#: the separation arithmetic.
CLEAR_GAP_MARKERS = 6
TRAIN_EXTENT_MARKERS = 2
#: Peer envelope fast bound used by the staleness expansion of 3.12.2, mm/ms.
V_PEER_MAX_MM_PER_MS = 0.305
#: Permitted peer report staleness before a peer counts as unseen (0031).
CTO_PEER_STALE_MS = 5000

# --- 7.2 / 7.4 movement -----------------------------------------------------
ACQ_SPEED_PWM = 60
RECOVER_WINDOW_MS = 90000
RECOVER_WINDOW_OBS = 40

# --- 7.4.1 hysteresis -------------------------------------------------------
SPEED_HYST_EVENTS_DOWN = 2
SPEED_HYST_EVENTS_UP = 5
SPEED_STEP_MIN_PCT = 10


def nominal_speed(pwm):
    """Piecewise-linear interpolation of the uncalibrated nominal table."""
    knots = sorted(NOMINAL_SPEED_MM_PER_MS)
    if pwm <= knots[0]:
        return NOMINAL_SPEED_MM_PER_MS[knots[0]]
    if pwm >= knots[-1]:
        return NOMINAL_SPEED_MM_PER_MS[knots[-1]]
    for a, b in zip(knots, knots[1:]):
        if a <= pwm <= b:
            va = NOMINAL_SPEED_MM_PER_MS[a]
            vb = NOMINAL_SPEED_MM_PER_MS[b]
            return va + (vb - va) * (pwm - a) / float(b - a)
    return NOMINAL_SPEED_MM_PER_MS[knots[-1]]     # pragma: no cover
