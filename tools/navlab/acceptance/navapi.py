"""The contract the replacement navigator must satisfy, and the loader.

THIS MODULE CONTAINS NO NAVIGATOR. It defines the interface the acceptance
families drive, the policy record for operator decisions that remain open, and
a loader that returns None until an implementation is supplied. Families that
need a navigator report NOT_IMPLEMENTED rather than passing vacuously.

Deliberately absent: any default/stub navigator. A stop-only navigator would
satisfy every safety gate and no usefulness gate, and the harness self-tests
prove the suite rejects one (selftests.stop_only).
"""
import os
import importlib
from dataclasses import dataclass, field

# --- startup modes, spec 4.0 -------------------------------------------------
MODE_EXACT = 'exact'                  # 1: MM/interval declaration
MODE_LAUNCH_REGION = 'launch_region'  # 2: launch region + orientation
MODE_UNKNOWN = 'unknown'              # 3: position unknown

# --- nav states, spec 4 ------------------------------------------------------
POSITIONED = 'POSITIONED'
ACQUIRING_ORIENTED = 'ACQUIRING_ORIENTED'
RECOVERING = 'RECOVERING'
UNLOCATED = 'UNLOCATED'

# --- movement states, spec 7.1 ----------------------------------------------
FULL_AUTHORITY = 'FULL_AUTHORITY'
RECOVERING_WITH_AUTHORITY = 'RECOVERING_WITH_AUTHORITY'
SPEED_LIMITED_FOR_UNCERTAINTY = 'SPEED_LIMITED_FOR_UNCERTAINTY'
MANUAL_NO_POSITION = 'MANUAL_NO_POSITION'
STOPPED_FOR_NAVIGATION_SAFETY = 'STOPPED_FOR_NAVIGATION_SAFETY'

MOVEMENT_STATES = {FULL_AUTHORITY, RECOVERING_WITH_AUTHORITY,
                   SPEED_LIMITED_FOR_UNCERTAINTY, MANUAL_NO_POSITION,
                   STOPPED_FOR_NAVIGATION_SAFETY}

#: The only navigation-commanded motion order permitted by spec 7.8.
ONLY_MOTION_ORDER = STOPPED_FOR_NAVIGATION_SAFETY

#: Names that must not appear anywhere in a navigator implementation (7.7).
PROHIBITED_SYMBOLS = ('LAUNCH_HOLD', 'launch_hold', 'LaunchHold')


@dataclass
class Policy:
    """Operator decisions that remain OPEN are configuration, not assumptions.

    Spec section 9 lists six. Each open one appears here so a family can be run
    under either ruling, and so no test silently bakes in an answer the
    operator has not given. Ruled-on items are NOT configurable.
    """
    # open decision 1 - fleet stop / yield behaviour
    fleet_stop_on_unbounded_occupancy: bool = True
    uncertain_locomotive_yields: bool = True
    # open decision 2 - station behaviour on entry to RECOVERING
    station_final_completes_on_recovering: bool = True
    # open decision 3 - command semantics
    orientation_command_available: bool = True
    launch_region_command_available: bool = True
    orientation_command_allowed_while_moving: bool = False
    # open decision 4 - protected-region declaration mechanism
    protected_region_mechanism: str = 'none'   # 'none' | 'config' | 'operator'
    # open decision 5 - telemetry / CTO occupancy representation
    occupancy_publication: str = 'multi_arc'   # 'multi_arc' | 'single_arc'
    occupancy_arcs_max: int = 3
    route_wide_fraction: float = 0.6
    # open decision 6 - dt discontinuity strategy
    discontinuity_strategy: str = 'A'          # 'A' | 'B'

    # --- ruled 2026-08-22, NOT open; present so tests can assert them --------
    unlocated_manual_movement_permitted: bool = True    # ruling: option A
    automatic_unlocated_crawl: bool = False             # ruling: option A
    launch_region_presumed: bool = False                # ruling: never presumed

    def open_decisions(self):
        return ['fleet_stop/yield', 'station_final_on_recovering',
                'command_semantics', 'protected_region_mechanism',
                'occupancy_publication', 'discontinuity_strategy']


@dataclass
class Detection:
    """One RAW detection, spec 3.2. No `dt` field: elapsed is branch-local.

    The three decoy_* fields are contamination bait. A conforming navigator
    ignores them entirely; invariants.ContaminationProbe proves it by running
    the same stream twice with different decoy values and requiring identical
    behaviour.
    """
    t_detect: int                 # 64-bit extended monotonic ms
    clock_epoch: int
    polarity: str                 # 'N' or 'S'
    peak: int
    duration_ms: int
    pwm_actual_history: list      # [(offset_ms_before_t_detect, pwm)]
    baseline_drift: int = 0
    decoy_firmware_mm: int = None
    decoy_firmware_verdict: str = None
    decoy_mqtt_recv_ts: int = None


@dataclass
class PeerReport:
    """What arrives over CTO. Motion fields may only ENLARGE or INVALIDATE a
    bound (spec 3.12.2); they can never create one, and `bounded_region` is the
    only field that may narrow anything."""
    t_report: int
    peer_id: str
    commanded_stopped: bool = None      # PEER_COMMANDED_STOPPED - telemetry only
    reported_speed_mm_per_ms: float = None
    reported_direction: str = None
    bounded_region: tuple = None        # PEER_BOUNDED(region) - authoritative only
    immobilised: bool = False           # PEER_IMMOBILISED - latched externally
    decoy_claimed_mm: int = None        # a peer's own navigation claim: not evidence


@dataclass
class NavStatus:
    """What the navigator must publish. Optional fields may be None; a family
    that needs a None field reports NOT_DEMONSTRATED, never PASS."""
    nav_state: str
    movement_state: str
    hypotheses: set = field(default_factory=set)   # {(mm, step)}
    complete: bool = True
    confirmation_authority: bool = True
    gap_bearing: bool = False
    confirmed_mm: int = None
    confirmed_dir: int = None
    just_confirmed: bool = False
    pending_depth: int = 0
    occupancy_arcs: list = None        # [(start_mm, end_mm)] published form
    speed_ceiling: float = None
    commanded_speed: float = None
    auto_running: bool = False
    station_armed: str = None
    station_substituted: str = None
    separation_claimable: bool = None
    manual_declaration_required: bool = False
    acquisition_context: str = None    # 'C1' | 'C2' | None (+ failing condition)
    seed_mode: str = None
    speed_reductions: int = 0
    speed_restorations: int = 0
    unscheduled_stops: int = 0
    branches: list = None              # optional: [(last_genuine, epoch, frozenset)]


class NavigatorContract:
    """Method surface the acceptance families drive. Documentation only."""

    def start(self, mode, policy, mm=None, direction=None): raise NotImplementedError
    def observe(self, detection): raise NotImplementedError
    def peer_report(self, report): raise NotImplementedError
    def operator(self, command, **kw): raise NotImplementedError
    def status(self): raise NotImplementedError


MISSING_IMPLEMENTATION = (
    'no replacement navigator exists yet; set NGR_NAVIGATOR=<module:factory> '
    'once one is implemented per docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md')


def load_navigator():
    """Returns a zero-arg factory, or None when no implementation is supplied.

    None is the expected state at the time this harness was frozen. It causes
    every navigator-dependent family to report NOT_IMPLEMENTED with a named
    reason -- never PASS.
    """
    spec = os.environ.get('NGR_NAVIGATOR')
    if not spec:
        return None
    mod_name, _, attr = spec.partition(':')
    mod = importlib.import_module(mod_name)
    return getattr(mod, attr or 'Navigator')
