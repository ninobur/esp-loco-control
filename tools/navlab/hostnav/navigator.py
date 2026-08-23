"""The replacement autonomous-acquisition host navigator.

Implements `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md` against the frozen
acceptance harness in `tools/navlab/acceptance/`. It is a host model. It
authorises no firmware change, no flash and no train run.

Structure
---------
`route`      the committed map, geometry, and the computed uniqueness lengths
`params`     engineering parameters, all uncalibrated (specification 10)
`envelopes`  robust timing-envelope interface and the PWM ring
`branches`   propagation, branch-local elapsed time, the branch list
`occupancy`  conservative occupancy, peer bounds, separation
this module  the four states, confirmation, movement authority, telemetry

One authoritative hypothesis set
--------------------------------
There is exactly one branch list, and its union is `H`: the set that is
published, the set S2 measures completeness against, and the set 4.1 requires
to be a singleton in `POSITIONED`. Nothing wider is kept alongside it.

Propagation preserves direction exactly as 3.5.1 writes it, and a declared
orientation therefore excludes the opposite plane until an explicit reversal
arrives (P10). A native reversal is **commanded motion state**, delivered
through `direction_changed(direction)`: it says which way the locomotive is
now going and nothing about where it is, so it rotates the existing
hypotheses in place -- markers preserved, direction changed -- and costs
neither completeness nor `|H| = 1`. The evidence window is dropped at the same
moment, because a polarity string spanning a reversal is not a route string
and must never read as unique.

Confirmation is by 3.11 **uniqueness only**. The collapse path the
specification also permits is deliberately not used as a confirmation source
in this build: it is the weaker of the two, and uniqueness alone meets every
usefulness gate on the generated families.

What is never consumed: a firmware position label, a firmware verdict, an MQTT
receipt time, a peer's own navigation claim, or any precomputed per-event dt
(P4, 3.2, 3.6, 3.12.2).
"""
import copy

from tools.navlab.acceptance import navapi as A

from . import params as P
from . import route as R
from .branches import Lane
from .envelopes import NominalEnvelope, PwmHistory
from .occupancy import (covering_arcs, peer_occupancy, route_wide, separated)

FULL_PWM = 100.0

SEED_LAUNCH_REGION = 'ACQ_LAUNCH_REGION'
SEED_ROUTE_WIDE = 'ACQ_ROUTE_WIDE'


class _Obs(object):
    """Internal view of one raw detection: the fields 3.2 permits, only."""

    __slots__ = ('t_detect', 'clock_epoch', 'pol_bit')

    def __init__(self, detection):
        self.t_detect = detection.t_detect
        self.clock_epoch = detection.clock_epoch
        self.pol_bit = R.pol_bit(detection.polarity)


class Navigator(object):
    """Host model of the replacement navigator. Zero-argument factory."""

    def __init__(self, envelope=None):
        self.env = envelope or NominalEnvelope()
        self.policy = A.Policy()
        self.history = PwmHistory()
        self.lane = Lane()

        self.nav_state = A.UNLOCATED
        self.movement_state = A.MANUAL_NO_POSITION
        self.confirmed = None
        self.declared_dir = None
        self.dir_invalidated = True
        self.gap_bearing = False

        self._mode = None
        self._seed_mode = None
        self._own_bound = None          # authoritative bound on our occupancy
        self._anchored = False
        self._anchor_provenance = None
        self._anchor_t = 0
        self._fault = False
        self._snapshot = None
        self._epoch = None

        self._window = []               # accepted detections: (pol bit, clean)
        self._just_confirmed = False
        self._obs_since_anchor = 0

        self.peers = {}
        self.authorised_pwm = 0.0
        self.manual_throttle_pwm = 0.0
        self.manual = False
        self.auto_requested = False
        self.motion_observed = False
        self.intended_station = None
        self.station_armed = None
        self.station_substituted = None

        self._eff_ceiling = FULL_PWM
        self._down = 0
        self._up = 0
        self._commanded = 0.0
        self._travel_dir = None
        self._ceiling_reason = None
        self._context = None
        self._acq_authorised = False
        self._stop_ordered = False
        self._membership_confirms_alone = False

        self.speed_reductions = 0
        self.speed_restorations = 0
        self.unscheduled_stops = 0
        self.native_reversals = 0
        self.confirmations = 0
        self.t_now = 0

    # ------------------------------------------------------------------ start
    def start(self, mode, policy, mm=None, direction=None):
        """Startup selection, specification 4.0. The launch region is never
        presumed: a boot with no selection is mode 3, not mode 2."""
        self.policy = policy or A.Policy()
        self._mode = mode
        self._window = []
        if mode == A.MODE_EXACT:
            if mm is None:
                raise ValueError('mode 1 requires a declared MM or interval')
            step = direction if direction in R.DIRS else R.CW
            self._anchor({(mm % R.DNA_N, step)}, 'OPERATOR_DECLARED')
            self.declared_dir = self._travel_dir = step
            self.dir_invalidated = False
            self._own_bound = {mm % R.DNA_N}
            self._seed_mode = None
            self.nav_state = A.POSITIONED
            self.confirmed = (mm % R.DNA_N, step)
        elif mode == A.MODE_LAUNCH_REGION:
            step = direction if direction in R.DIRS else R.CW
            self._seed({(m, step) for m in R.LAUNCH_REGION})
            self.declared_dir = self._travel_dir = step
            self.dir_invalidated = False
            self._own_bound = set(R.LAUNCH_REGION)
            self._seed_mode = SEED_LAUNCH_REGION
            self.nav_state = A.ACQUIRING_ORIENTED
        else:
            if direction in R.DIRS:
                # Orientation declared, position unknown: 4.2 ACQ_ROUTE_WIDE.
                self._seed(R.all_hypotheses(direction))
                self.declared_dir = self._travel_dir = direction
                self.dir_invalidated = False
                self._seed_mode = SEED_ROUTE_WIDE
                self.nav_state = A.ACQUIRING_ORIENTED
            else:
                # Nothing declared: 4.4 UNLOCATED, all 342 bits.
                self._seed(R.all_hypotheses())
                self.declared_dir = None
                self.dir_invalidated = True
                self._seed_mode = None
                self.nav_state = A.UNLOCATED
            self._own_bound = None
        self._update_movement()
        return self

    def _seed(self, cands):
        self.lane.seed(set(cands), 0, None, origin=False)
        self._anchored = False
        self._obs_since_anchor = 0

    def _anchor(self, cands, provenance):
        origin = self.t_now > 0
        self.lane.seed(set(cands), self.t_now, self._epoch, origin=origin)
        self._anchored = True
        self._anchor_provenance = provenance
        self._anchor_t = self.t_now
        self._obs_since_anchor = 0
        self.gap_bearing = False
        self._fault = False

    # ---------------------------------------------------------------- observe
    def observe(self, detection):
        """One raw detection, specification 3.2. No dt field is read: elapsed
        is a property of an (event, branch) pair and is computed per 3.6."""
        self._just_confirmed = False
        self.t_now = detection.t_detect
        self._epoch = detection.clock_epoch
        self._obs_since_anchor += 1
        self.history.add_detection(detection.t_detect,
                                   detection.pwm_actual_history)
        obs = _Obs(detection)
        ghost_like = not (detection.peak >= P.PEAK_FLOOR
                          and detection.duration_ms >= P.DUR_FLOOR)

        skip_t, gap_t = self.lane.observe(obs, self.env, self.history,
                                          ghost_like)

        if gap_t:
            # Case I, 6.2 strategy A: elapsed unknown, an honest
            # over-approximation, confirmation authority suspended until 3.11
            # uniqueness clears it. Direction may also have changed during the
            # unknown time, so it is no longer held.
            self.gap_bearing = True
            self.dir_invalidated = True
            self._window = []

        if not self.lane.alive():
            self._contradiction()
        elif not ghost_like:
            clean = (not skip_t and not gap_t
                     and self.lane.pending_depth == 0
                     and self.lane.complete)
            self._window.append((obs.pol_bit, clean))
            if len(self._window) > 4 * R.W_BOTH:
                del self._window[0]
            self.motion_observed = True

        self._try_confirm()
        self._update_state()
        self._update_station()
        self._update_movement()

    def direction_changed(self, direction):
        """A native reversal. Commanded motion state, not evidence.

        It carries no position: every hypothesis keeps its marker and takes
        the new travel direction, so `H` neither grows nor shrinks and a
        locomotive that was `POSITIONED` stays `POSITIONED` at the same
        marker, now going the other way. It re-anchors nothing.

        The evidence window is dropped, because the observed polarity string
        is not a route string across a reversal and a would-be unique window
        spanning one must not confirm (3.11, T4).
        """
        if direction not in R.DIRS or direction == self._travel_dir:
            return False
        self.lane.reverse(direction)
        self._travel_dir = direction
        self.declared_dir = direction
        self.dir_invalidated = False
        self._window = []
        self.native_reversals += 1
        self._update_state()
        self._update_station()
        self._update_movement()
        return True

    def _contradiction(self):
        """Specification 5. Every branch empty means the model is wrong.
        Never invent a position, never permanently halt."""
        self._snapshot = dict(
            t=self.t_now, anchor=self.confirmed,
            provenance=self._anchor_provenance,
            branches=[(b.last_genuine, b.epoch, sorted(b.h))
                      for b in self.lane.branches],
            binding_constraint=self._ceiling_reason,
            peers={k: vars(v) for k, v in self.peers.items()})
        self._stop_ordered = True
        self._fault = True
        self._anchored = False
        self._seed_mode = None
        self._own_bound = None
        self.declared_dir = None
        self.dir_invalidated = True
        self.gap_bearing = False
        self._window = []
        self._travel_dir = None
        self.lane.seed(R.all_hypotheses(), self.t_now, self._epoch, origin=True)

    # ----------------------------------------------------------- confirmation
    def published(self):
        return self.lane.union

    def _try_confirm(self):
        """Specification 3.11, uniqueness path. All four conditions."""
        if self._fault:
            # A contradiction is an operational failure requiring diagnosis
            # (5, 7.6) and 7.5 names it as one of the four cases that need an
            # operator restart. The navigator keeps reasoning and publishing;
            # it does not re-anchor itself out of a falsified model unasked.
            return
        if not self.lane.complete:
            return                                   # condition 1: COMPLETE
        if self.lane.pending_depth:
            return
        if self._anchored and len(self.lane.union) == 1:
            # Already positioned and unambiguous: successor agreement
            # maintains SELF_CONFIRMED (4.1), it is not a new confirmation.
            return
        if self.declared_dir is not None and not self.dir_invalidated:
            w, table = R.W_DIR, R.UNIQUE_DIR[self.declared_dir]
        else:
            w, table = R.W_BOTH, R.UNIQUE_BOTH
        if len(self._window) < w:
            return
        tail = self._window[-w:]
        if not all(clean for _, clean in tail):
            return
        match = table.get(tuple(bit for bit, _ in tail))
        if not match or len(match) != 1:
            return                                   # condition 3: uniqueness
        cand = match[0]
        if cand not in self.published():
            return                                   # condition 4: inside H
        self.confirmed = cand
        self.declared_dir = cand[1]
        self.dir_invalidated = False
        self.confirmations += 1
        self._just_confirmed = True
        self._anchor({cand}, 'SELF_CONFIRMED')

    # ------------------------------------------------------------------ state
    def _update_state(self):
        if self._fault:
            self.nav_state = A.UNLOCATED
            self.confirmed = None
            return
        track = self.lane.union
        if self._anchored and len(track) == 1 and self.lane.complete:
            # 4.1: POSITIONED means one candidate in a set known to contain
            # the truth. Both halves, or neither.
            self.nav_state = A.POSITIONED
            self.confirmed = next(iter(track))
            return
        self.confirmed = None
        if self._anchored:
            occ = R.occupancy_markers(track, 0)
            expired = (self._obs_since_anchor > P.RECOVER_WINDOW_OBS
                       or self.t_now - self._anchor_t > P.RECOVER_WINDOW_MS)
            if route_wide(occ) or expired:
                # 6.5: strategy A carries strategy B's outcome as its floor.
                self._anchored = False
                self._seed_mode = None
                self._own_bound = None
                self.dir_invalidated = True
                self.nav_state = A.UNLOCATED
            else:
                self.nav_state = A.RECOVERING
            return
        self.nav_state = (A.ACQUIRING_ORIENTED if self._seed_mode
                          else A.UNLOCATED)

    # --------------------------------------------------------------- stations
    def _update_station(self):
        """7.3 first-station lookahead. A station approach needs an exact
        distance to centre, so below POSITIONED the capability is
        **unavailable**, not inhibited: nothing is held and nothing has to be
        released."""
        self.station_armed = None
        self.station_substituted = None
        if self.intended_station is None or self.nav_state != A.POSITIONED:
            return
        mm, step = self.confirmed
        centre = R.STATIONS.get(self.intended_station)
        if centre is None:
            return
        if R.markers_ahead(mm, centre, step) >= R.STATION_LOOKAHEAD_MARKERS:
            self.station_armed = self.intended_station
        else:
            self.station_substituted = R.next_station(
                mm, step, R.STATION_LOOKAHEAD_MARKERS)

    # --------------------------------------------------------------- movement
    def _own_occupancy_bounded(self, published):
        """4.2 C2 condition 1: our own occupancy independently bounded."""
        if not self.lane.complete:
            return False
        if self.nav_state == A.POSITIONED:
            return True
        if self.nav_state == A.UNLOCATED:
            return False
        if self._seed_mode == SEED_ROUTE_WIDE:
            return False
        if self._own_bound is None and self.nav_state != A.RECOVERING:
            return False
        return not route_wide(R.occupancy_markers(published, 0))

    def _peer_hazard(self, ours):
        """Returns (hazard, reason, every_peer_bounded)."""
        if not self.peers:
            return False, None, True
        all_bounded = True
        for rep in self.peers.values():
            theirs = peer_occupancy(rep, self.t_now)
            if theirs is None:
                all_bounded = False
                if self.policy.fleet_stop_on_unbounded_occupancy:
                    return True, 'peer occupancy unbounded', False
                continue
            if not separated(ours, theirs):
                return True, 'decision 0033 separation not provable', all_bounded
        return False, None, all_bounded

    def _update_movement(self):
        published = self.published()
        ours = R.occupancy_markers(published, P.TRAIN_EXTENT_MARKERS)
        own_bounded = self._own_occupancy_bounded(published)
        hazard, reason, peers_bounded = self._peer_hazard(ours)
        if self.peers and not own_bounded \
                and self.policy.fleet_stop_on_unbounded_occupancy:
            hazard, reason = True, 'own occupancy unbounded or incomplete'

        self._apply_hysteresis(0.0 if hazard else FULL_PWM)
        self._ceiling_reason = reason
        manual = self.manual_throttle_pwm if self.manual else 0.0

        if self.nav_state == A.UNLOCATED:
            # 4.4.2 ruling A: no automatic crawl. The navigator authorises no
            # motion from an unknown position, and withholds none either --
            # the operator has the throttle (4.4.3).
            self._context = None
            self._commanded = manual
            self.movement_state = (A.STOPPED_FOR_NAVIGATION_SAFETY
                                   if self._stop_ordered
                                   else A.MANUAL_NO_POSITION)
            return

        if self.nav_state == A.ACQUIRING_ORIENTED:
            permitted = self._acquisition_context(own_bounded, peers_bounded,
                                                  hazard)
            if permitted:
                # 4.2: motion is required to acquire, and where a context
                # permits it, it is at ACQ_SPEED with no AUTO mission. This is
                # the navigator's own authorisation and it survives into
                # POSITIONED as the speed 7.2 ramps from.
                self._acq_authorised = True
                self._commanded = min(float(P.ACQ_SPEED_PWM), self._eff_ceiling)
                self.movement_state = (
                    A.SPEED_LIMITED_FOR_UNCERTAINTY
                    if self._eff_ceiling < P.ACQ_SPEED_PWM
                    else A.RECOVERING_WITH_AUTHORITY)
            elif manual:
                self._commanded = manual
                self.movement_state = A.MANUAL_NO_POSITION
            else:
                # The locomotive stands and publishes the reason. No order is
                # issued and no latch is set; motion becomes authorised the
                # moment the conditions are met (4.2).
                self._commanded = 0.0
                self.movement_state = A.SPEED_LIMITED_FOR_UNCERTAINTY
            return

        self._context = None
        base = max(self.authorised_pwm, manual,
                   float(P.ACQ_SPEED_PWM) if self._acq_authorised else 0.0)
        self._commanded = min(base, self._eff_ceiling)
        if hazard or self._stop_ordered:
            if self.movement_state != A.STOPPED_FOR_NAVIGATION_SAFETY:
                self.unscheduled_stops += 1
            self._commanded = 0.0
            self.movement_state = A.STOPPED_FOR_NAVIGATION_SAFETY
        elif self._eff_ceiling < base:
            self.movement_state = A.SPEED_LIMITED_FOR_UNCERTAINTY
        elif self.nav_state == A.POSITIONED:
            self.movement_state = A.FULL_AUTHORITY
        else:
            self.movement_state = A.RECOVERING_WITH_AUTHORITY

    def _acquisition_context(self, own_bounded, peers_bounded, hazard):
        """4.2. Two contexts permit acquisition motion; there is no third.

        C1 requires establishing that no peer is enlisted, by the membership
        rules of decision 0031 -- **absence may not be inferred from
        silence**. The navigator contract carries no membership channel, so on
        generated evidence C1 is never established and C2 is the only context
        that authorises acquisition motion. That is the conservative reading
        and it is recorded rather than assumed away.
        """
        if self._membership_confirms_alone:
            self._context = 'C1'
            return True
        if not self.peers:
            self._context = ('none: C1 unavailable, absence of a peer is not '
                             'established by the membership rules of decision '
                             '0031 and may not be inferred from silence')
            return False
        if not own_bounded:
            self._context = ('none: C2 condition 1 unsatisfied, our own '
                             'occupancy is not independently bounded')
            return False
        if not peers_bounded:
            self._context = ('none: C2 condition 2 unsatisfied, a peer '
                             'occupancy is unbounded')
            return False
        if hazard:
            self._context = ('none: C2 condition 3 unsatisfied, decision 0033 '
                             'separation is not provable for every candidate '
                             'pair')
            return False
        self._context = 'C2'
        return True

    def _apply_hysteresis(self, raw):
        """7.4.1. One unresolved detection may not cause crawl/cruise cycling.
        A constraint that would be violated before the next detection arrives
        is acted on immediately: hysteresis delays discretionary reductions,
        never a genuine safety brake."""
        if raw < self._eff_ceiling:
            self._up = 0
            drop_pct = 100.0 * (self._eff_ceiling - raw) / max(self._eff_ceiling, 1.0)
            if raw <= 0.0:
                if self._eff_ceiling > 0:
                    self.speed_reductions += 1
                self._eff_ceiling = raw
                self._down = 0
            elif drop_pct >= P.SPEED_STEP_MIN_PCT:
                self._down += 1
                if self._down >= P.SPEED_HYST_EVENTS_DOWN:
                    self._eff_ceiling = raw
                    self._down = 0
                    self.speed_reductions += 1
        elif raw > self._eff_ceiling:
            self._down = 0
            self._up += 1
            if self._up >= P.SPEED_HYST_EVENTS_UP:
                self._eff_ceiling = raw
                self._up = 0
                self.speed_restorations += 1
        else:
            self._down = self._up = 0

    # ------------------------------------------------------------------ peers
    def peer_report(self, report):
        """3.12.2. Motion fields may enlarge or invalidate a bound. They can
        never create one, and never independently grant authority."""
        self.peers[report.peer_id] = report
        self._update_movement()

    # --------------------------------------------------------------- operator
    def operator(self, command, **kw):
        """7.5. The operator declares orientation, launch region, an exact MM
        after deliberate stationary identification, initial movement, STOP and
        restart. The operator does not confirm a self-acquired position and
        supplies no routine post-recovery GO."""
        self._just_confirmed = False
        if command == 'declare_mm':
            if not kw.get('stationary'):
                # 7.5: a declaration issued while the navigator observes
                # motion is rejected with a reason, not silently accepted.
                self._declaration_refusal = (
                    'declaration refused: it must record deliberate stationary '
                    'placement or direct stationary verification')
                return False
            mm = kw['mm'] % R.DNA_N
            step = kw.get('direction')
            if step not in R.DIRS:
                step = self.declared_dir if self.declared_dir in R.DIRS else R.CW
            self._anchor({(mm, step)}, 'OPERATOR_DECLARED')
            self.declared_dir = step
            self.dir_invalidated = False
            self._own_bound = {mm}
            self._seed_mode = None
            self._window = []
            self.confirmed = (mm, step)
            self.nav_state = A.POSITIONED
            self._update_station()
            self._update_movement()
            return True
        if command == 'declare_orientation':
            step = kw.get('direction')
            if step not in R.DIRS:
                return False
            if not self.policy.orientation_command_available:
                return False
            if self.motion_observed and \
                    not self.policy.orientation_command_allowed_while_moving:
                return False
            self._seed(R.all_hypotheses(step))
            self.declared_dir = step
            self.dir_invalidated = False
            self._seed_mode = SEED_ROUTE_WIDE
            self._own_bound = None
            self._window = []
            self.nav_state = A.ACQUIRING_ORIENTED
            self._update_movement()
            return True
        if command == 'select_launch_region':
            if not self.policy.launch_region_command_available:
                return False
            step = kw.get('direction')
            if step not in R.DIRS:
                return False
            self._seed({(m, step) for m in R.LAUNCH_REGION})
            self.declared_dir = step
            self.dir_invalidated = False
            self._seed_mode = SEED_LAUNCH_REGION
            self._own_bound = set(R.LAUNCH_REGION)
            self._window = []
            self.nav_state = A.ACQUIRING_ORIENTED
            self._update_movement()
            return True
        if command == 'authorise_speed':
            self.authorised_pwm = float(kw.get('pwm', 0))
            self._update_movement()
            return True
        if command == 'manual_throttle':
            # 4.4.3 / ruling A: manual operation without a declared position is
            # a supported operating condition, not a fault and not a held
            # state. The navigator neither commands motion nor blocks it.
            if not self.policy.unlocated_manual_movement_permitted:
                return False
            self.manual = True
            self.manual_throttle_pwm = float(kw.get('pwm', 0))
            self._update_movement()
            return True
        if command == 'auto':
            self.auto_requested = bool(kw.get('on', True))
            self._update_movement()
            return True
        if command == 'stop':
            self.authorised_pwm = 0.0
            self.manual_throttle_pwm = 0.0
            self._commanded = 0.0
            self._update_movement()
            return True
        if command == 'restart':
            # 7.5: a GO is required only after the locomotive actually
            # stopped, entered fleet hold, hit a contradiction, or explicitly
            # cancelled AUTO. This is that GO, and nothing else needs one.
            self._fault = False
            self._stop_ordered = False
            self._update_state()
            self._update_movement()
            return True
        if command == 'declare_alone':
            # Decision 0031 membership statement: the only thing that can
            # establish C1. Silence never does.
            self._membership_confirms_alone = bool(kw.get('alone', True))
            self._update_movement()
            return True
        if command == 'set_intended_station':
            name = kw.get('name')
            if name not in R.STATIONS:
                return False
            self.intended_station = name
            self._update_station()
            return True
        if command == 'force_hypotheses':
            # Diagnostic injection used by the occupancy-publication families.
            # It shapes the candidate set directly so the published form can
            # be measured against the bitmap; it is not a navigation input.
            cands = {tuple(h) for h in kw.get('hypotheses') or ()}
            self.lane.seed(set(cands), self.t_now, self._epoch, origin=True)
            self._update_movement()
            return True
        return False

    # ----------------------------------------------------------------- status
    def status(self):
        published = self.published()
        occ = R.occupancy_markers(published, P.TRAIN_EXTENT_MARKERS)
        complete = self.lane.complete
        if self.policy.occupancy_publication == 'single_arc':
            arcs = covering_arcs(occ, 1)
        else:
            arcs = covering_arcs(occ, self.policy.occupancy_arcs_max
                                 or P.OCC_ARCS_MAX)
        base = max(self.authorised_pwm,
                   self.manual_throttle_pwm if self.manual else 0.0,
                   float(P.ACQ_SPEED_PWM) if self._acq_authorised else 0.0)
        limited = self._commanded < base
        peers_bounded = all(peer_occupancy(r, self.t_now) is not None
                            for r in self.peers.values()) if self.peers else False
        branches = [(b.last_genuine, b.epoch, frozenset(b.h))
                    for b in self.lane.branches]
        return A.NavStatus(
            nav_state=self.nav_state,
            movement_state=self.movement_state,
            hypotheses=set(published),
            complete=complete,
            confirmation_authority=(complete and not self.gap_bearing
                                    and not self._fault),
            gap_bearing=self.gap_bearing,
            confirmed_mm=self.confirmed[0] if self.confirmed else None,
            confirmed_dir=self.confirmed[1] if self.confirmed else None,
            just_confirmed=self._just_confirmed,
            pending_depth=self.lane.pending_depth,
            occupancy_arcs=arcs,
            speed_ceiling=float(self._eff_ceiling) if limited else None,
            commanded_speed=float(self._commanded),
            auto_running=bool(self.auto_requested
                              and self.nav_state == A.POSITIONED),
            station_armed=self.station_armed,
            station_substituted=self.station_substituted,
            separation_claimable=bool(
                self.peers and peers_bounded and complete
                and self._own_occupancy_bounded(published)),
            manual_declaration_required=False,
            acquisition_context=self._context,
            seed_mode=self._seed_mode,
            speed_reductions=self.speed_reductions,
            speed_restorations=self.speed_restorations,
            unscheduled_stops=self.unscheduled_stops,
            branches=branches)

    # ------------------------------------------------------------ diagnostics
    def diagnostic_snapshot(self):
        """7.6: the latched record of an unscheduled navigation stop."""
        return copy.deepcopy(self._snapshot)
