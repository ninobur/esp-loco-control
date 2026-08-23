"""Hypothesis propagation and the branch list -- specification 3.5, 3.6, 3.10.

Two ideas do all the work here.

**Local propagation (3.5.1).** A detection is converted, via branch-local
elapsed time and the timing envelope, into a distance window `[d_lo, d_hi]`.
Every pole-matching marker whose distance from a live candidate falls in that
window is admitted. A missed marker needs no special case: it is simply a
larger window. Both ends are over-approximations, so a set that contained the
truth still contains it (P5).

**Branch-local elapsed time (3.6).** Each branch keeps its own `last_genuine`
timestamp and its own clock epoch. The genuine side of a fork advances it; the
phantom side does not. No elapsed interval is folded into a successor and none
is counted twice. Elapsed never depends on whether firmware accepted an event
and never on an MQTT receipt time (P4).
"""
from . import params as P
from .route import DNA, DNA_N, POL_MARKERS, nxt, step_mm

#: Propagation modes for one (detection, branch) pair.
KNOWN = 'known'         # elapsed known: envelope window
FIRST = 'first'         # branch has no timing origin yet: at most one interval
UNKNOWN = 'unknown'     # 6.2 strategy A: d_lo = 0, d_hi = infinity


def propagate(cands, pol, mode, d_lo, d_hi, stopped=False):
    """Returns (new candidate set, skip_admitted).

    Direction is **preserved**, exactly as 3.5.1 writes it: `dir(q) = dir(p)`.
    A detection never changes travel direction and never introduces the
    opposite plane. A native reversal is commanded motion state and arrives
    through `Navigator.direction_changed`, which rotates the existing
    hypotheses rather than widening them.

    `skip_admitted` records that some candidate was advanced by more than one
    marker, i.e. a missed marker was admitted. Specification 3.11 refuses a
    uniqueness window containing one, because the observed string is then not
    the route string.
    """
    out = set()
    skip = False
    for p, d in cands:
        for dd in (d,):
            if mode is UNKNOWN:
                for q in POL_MARKERS[pol]:
                    out.add((q, dd))
                skip = True
                continue
            if mode is FIRST:
                lo = 0.0
                hi = 0.0 if stopped else step_mm(p, dd) * P.SPEED_BAND_HI
            else:
                lo, hi = d_lo, d_hi
            if stopped and DNA[p] == pol:
                # Same-magnet reread. Admitted only where the PWM profile
                # leaves standstill possible: repeated identical evidence is
                # one observation, never several (3.5.1, T8).
                out.add((p, dd))
            dist = 0.0
            q = p
            for n in range(1, DNA_N + 1):
                dist += step_mm(q, dd)
                q = nxt(q, dd)
                if dist > hi:
                    break
                if dist >= lo and DNA[q] == pol:
                    out.add((q, dd))
                    if n > 1:
                        skip = True
    return out, skip


class Branch:
    """One hypothesis bitmap plus its own timing origin (3.5, 3.6)."""

    __slots__ = ('h', 'last_genuine', 'epoch', 'origin', 'pending')

    def __init__(self, h, last_genuine, epoch, origin, pending=0):
        self.h = h
        self.last_genuine = last_genuine
        self.epoch = epoch
        self.origin = origin          # False until this branch has crossed one
        self.pending = pending

    def copy(self):
        return Branch(set(self.h), self.last_genuine, self.epoch, self.origin,
                      self.pending)

    def mode(self, detection):
        if not self.origin:
            return FIRST
        if detection.clock_epoch != self.epoch:
            return UNKNOWN
        return KNOWN


class Lane:
    """The branch list. There is exactly one, and it is authoritative.

    Its union is the hypothesis set `H` the navigator publishes, the set S2
    measures completeness against, and the set 4.1 requires to be a singleton
    in `POSITIONED`. There is no second, wider set held alongside it: a set
    that is published as `COMPLETE` and a set that is navigated on must be the
    same set, or `|H| = 1` and completeness mean different things.
    """

    def __init__(self):
        self.branches = []
        self.complete = True
        self.collapsed = False

    # -- native reversal, 4.1 / implementation map ---------------------------
    def reverse(self, direction):
        """Hypotheses preserved, travel direction reversed.

        Commanded motion state says which way the locomotive is now going and
        nothing about where it is, so every candidate keeps its marker and
        takes the new direction. The set neither grows nor shrinks, which is
        why a reversal costs no completeness and no `|H| = 1`.
        """
        for b in self.branches:
            b.h = {(mm, direction) for mm, _ in b.h}

    # -- seeding -------------------------------------------------------------
    def seed(self, cands, t=0, epoch=None, origin=False):
        self.branches = [Branch(set(cands), t, epoch, origin, 0)]
        self.complete = True
        self.collapsed = False

    @property
    def union(self):
        out = set()
        for b in self.branches:
            out |= b.h
        return out

    @property
    def pending_depth(self):
        return max([b.pending for b in self.branches] or [0])

    def alive(self):
        return any(b.h for b in self.branches)

    # -- one detection -------------------------------------------------------
    def observe(self, detection, envelope, history, ghost_like):
        """Returns (skip_admitted, gap_seen)."""
        pol = detection.pol_bit
        new = []
        skip_any = False
        gap_seen = False
        for b in self.branches:
            mode = b.mode(detection)
            d_lo = d_hi = 0.0
            stopped = False
            if mode is KNOWN:
                d_lo, d_hi = envelope.distance_window(
                    history, b.last_genuine, detection.t_detect)
                stopped = envelope.stopped(history, b.last_genuine,
                                           detection.t_detect)
            elif mode is FIRST:
                stopped = envelope.stopped(history, detection.t_detect - 1,
                                           detection.t_detect)
            else:
                gap_seen = True
            h2, skip = propagate(b.h, pol, mode, d_lo, d_hi, stopped)
            skip_any = skip_any or skip
            if ghost_like:
                if h2:
                    # Genuinely ambiguous: hold both, decide on the successor.
                    new.append(Branch(h2, detection.t_detect,
                                      detection.clock_epoch, True,
                                      b.pending + 1))
                    new.append(Branch(set(b.h), b.last_genuine, b.epoch,
                                      b.origin, b.pending + 1))
                else:
                    # Physics resolved it: no marker was reachable, so none was
                    # crossed. The phantom wins outright; nothing stays pending
                    # and no evidence was deleted (P8).
                    new.append(Branch(set(b.h), b.last_genuine, b.epoch,
                                      b.origin, 0))
            elif h2:
                new.append(Branch(h2, detection.t_detect,
                                  detection.clock_epoch, True, 0))
            # a branch whose propagation is empty dies (3.10)
        self.branches = [b for b in new if b.h]
        self._cap()
        return skip_any, gap_seen

    def _cap(self):
        """3.10: the branch list is the one capped structure. Overflow
        collapses it to its union and costs confirmation authority only."""
        if not self.branches:
            return
        over = (len(self.branches) > P.BRANCH_MAX
                or self.pending_depth > P.PENDING_DEPTH_MAX)
        if over:
            union = self.union
            oldest = min(b.last_genuine for b in self.branches)
            origin = all(b.origin for b in self.branches)
            epoch = self.branches[0].epoch
            self.branches = [Branch(union, oldest, epoch, origin, 0)]
            self.collapsed = True
            self.complete = False
        elif self.collapsed and len(self.branches) == 1 \
                and self.pending_depth == 0:
            self.collapsed = False
            self.complete = True
