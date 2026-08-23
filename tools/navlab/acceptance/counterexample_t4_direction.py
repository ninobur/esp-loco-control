#!/usr/bin/env python3
"""Minimal counterexample: T4 is unsatisfiable under the frozen contract.

    python3 -m tools.navlab.acceptance.counterexample_t4_direction

This is EVIDENCE, not a test family. It is not registered in
`families.REGISTRY`, it changes no expectation, and it runs no navigator --
neither the one under test nor any other. It establishes a property of the
committed map and the frozen `navapi` record schema alone, so that the harness
correction it justifies rests on something independent of the implementation
that happened to trip over it.

What is demonstrated
--------------------
There exist two physically realisable worlds that

* differ in ground truth -- one continues forward, the other has natively
  reversed -- and
* produce **detection records that are identical on every field the navigator
  is permitted to consume** (specification 3.2), and
* deliver no other input, because `NavigatorContract` exposes only
  `observe`, `peer_report`, `operator` and `status`, and nothing in the
  reversal stream calls `operator`.

Any navigator is a function of its inputs. Identical inputs therefore produce
an identical `status()` in both worlds -- including an identical `hypotheses`
set. Two frozen requirements then collide:

* **S2** (`invariants.Monitor.check_completeness`) requires the true
  `(marker, direction)` to be in `H` at every detection where `H` is
  `COMPLETE`.
* **Specification 4.1** requires `|H| = 1` in `POSITIONED`, and the frozen
  suite asserts that shape directly at `T0` and `T5`.

One single-element `H` cannot contain two different truths. So on this map a
conforming navigator must either leave `POSITIONED` on an ordinary clean
forward run, or under-approximate `H` in the reversed world. The first breaks
the `POSITIONED`-at-end requirement of `T1a`, `T1`, `T2`, `T3`, `T13` and
`T21`; the second breaks S2 and T4.

The gap is in the harness, not the design. `with_reversal` records the
reversal in `Stream.reversals`, and `Monitor.run` never delivers it. The
locomotive's own firmware knows its travel direction -- the implementation map
RETAINS `applyDirection()`, `motorDirection`, `sessionDir` and `navDir`
"including native reversal handling" -- so travel direction is available on
the device and is simply absent from the host contract. It is commanded motion
state, not evidence intrinsic to a magnet detection and not an authoritative
operator position declaration.

Exit status: 0 when the defect is demonstrated. Non-zero if the committed map
ever stops supporting the witness, in which case this evidence expires and the
harness correction must be re-justified rather than inherited.
"""
import sys

from . import ngrmap as M
from .generate import Generator, TruthEvent

#: Fields the navigator may consume (spec 3.2). The three decoy fields are
#: excluded deliberately: a conforming navigator ignores them, so a difference
#: there could not distinguish the worlds for any conforming navigator.
PERMITTED = ('t_detect', 'clock_epoch', 'polarity', 'peak', 'duration_ms',
             'pwm_actual_history', 'baseline_drift')

#: The generator's own physics tolerance, from `Generator._advance_time`.
JITTER_LO, JITTER_HI = 0.92, 1.08
PWM_SPEED = 0.130          # generate.SPEED[60], the bucket clean_run uses


def invisible_reversal_positions():
    """Markers where a native reversal changes neither polarity nor interval.

    Both must hold, or the record would differ somewhere the navigator may
    read: polarity is a direct field, and interval length shows up in elapsed.
    """
    out = []
    for p in range(M.DNA_N):
        if M.DNA[M.nxt(p, M.CW)] != M.DNA[M.nxt(p, M.CCW)]:
            continue
        if M.step_mm(p, M.CW) != M.step_mm(p, M.CCW):
            continue
        depth = 0
        while True:
            f = (p + depth + 1) % M.DNA_N
            b = (p - depth - 1) % M.DNA_N
            if M.DNA[f] != M.DNA[b]:
                break
            if M.step_mm((p + depth) % M.DNA_N, M.CW) != \
                    M.step_mm((p - depth) % M.DNA_N, M.CCW):
                break
            depth += 1
        out.append((p, depth))
    return out


def build_worlds(pivot, reverse_at, n):
    """Two truth labellings over ONE detection sequence.

    World A: the locomotive keeps going CW.
    World B: at `reverse_at` it reversed, and has been running CCW since.

    The detections are the same objects in both, which is the whole point: no
    reconstruction, no re-randomisation, nothing that could smuggle in a
    difference.
    """
    start = (pivot - reverse_at) % M.DNA_N
    stream = Generator(4343).clean_run(start, M.CW, n)
    world_a = [(e.true_mm, e.true_dir) for e in stream.events]
    world_b = []
    mm = start
    for i in range(n):
        step = M.CW if i < reverse_at else M.CCW
        mm = M.nxt(mm, step)
        world_b.append((mm, step))
    return stream, world_a, world_b


def check(label, ok, detail=''):
    print('  %s  %s%s' % ('OK  ' if ok else 'FAIL', label,
                          '' if ok else '  -- ' + detail))
    return ok


def main():
    print(__doc__.strip().splitlines()[0])
    print()
    ok = True

    witnesses = invisible_reversal_positions()
    deep = sorted(witnesses, key=lambda w: -w[1])
    ok &= check('the committed map admits an invisible reversal at all',
                bool(witnesses),
                'no marker hides a reversal in both polarity and interval')
    print('       %d of %d markers hide a reversal in both polarity and '
          'interval; deepest run %s' % (len(witnesses), M.DNA_N, deep[:3]))

    pivot, depth = deep[0]
    reverse_at = 3
    n = reverse_at + depth
    stream, world_a, world_b = build_worlds(pivot, reverse_at, n)
    print('       witness: MM%03d, %d indistinguishable detections after the '
          'reversal' % (pivot, depth))
    print('       world A truth: %s' % (world_a[reverse_at:],))
    print('       world B truth: %s' % (world_b[reverse_at:],))
    print()

    # 1. The two worlds genuinely disagree.
    ok &= check('the two worlds disagree about (marker, direction)',
                world_a[reverse_at:] != world_b[reverse_at:])

    # 2. World B is physically realisable, by the generator's own physics.
    realisable = True
    prev = (pivot - reverse_at) % M.DNA_N
    for i, ev in enumerate(stream.events):
        mm_b, step_b = world_b[i]
        if M.DNA[mm_b] != (1 if ev.detection.polarity == 'N' else 0):
            realisable = False
            break
        dist = M.step_mm(prev, step_b)
        jitter = ev.true_elapsed_ms / (dist / PWM_SPEED)
        if not JITTER_LO <= jitter <= JITTER_HI:
            realisable = False
            break
        prev = mm_b
    ok &= check('world B is realisable under the generator\'s own physics',
                realisable,
                'polarity or elapsed is inconsistent with the reversed path')

    # 3. The records are identical on every permitted field. They are the same
    #    objects, so this is a schema check as much as a data check: it fails
    #    the moment a direction-bearing field is added, which is exactly the
    #    correction this evidence justifies.
    schema_ok = all(hasattr(stream.events[0].detection, f) for f in PERMITTED)
    ok &= check('every permitted field of spec 3.2 exists on the record',
                schema_ok)
    carries_direction = [f for f in vars(stream.events[0].detection)
                         if 'dir' in f.lower()]
    ok &= check('no permitted field carries travel direction',
                not carries_direction,
                'record already carries %s' % carries_direction)

    # 4. The contract offers no other channel.
    from . import navapi as A
    surface = [m for m in dir(A.NavigatorContract) if not m.startswith('_')]
    ok &= check('NavigatorContract exposes no motion or direction method',
                not [m for m in surface
                     if 'dir' in m.lower() or 'motion' in m.lower()],
                'surface is %s' % surface)
    ok &= check('the reversal is recorded by the generator but never '
                'delivered',
                bool(Generator(1).with_reversal(43, M.CW, 3, 3).reversals))

    # 5. Exhaustive: no single-element H satisfies S2 in both worlds.
    survivors = [h for h in
                 [(m, d) for m in range(M.DNA_N) for d in M.DIRS]
                 if h == world_a[reverse_at] and h == world_b[reverse_at]]
    ok &= check('no single-element H satisfies S2 in both worlds '
                '(all %d enumerated)' % (2 * M.DNA_N), not survivors)

    print()
    if ok:
        print('DEFECT DEMONSTRATED. Under the frozen contract, S2 and '
              'specification 4.1 cannot both hold at a reversal.')
        print('The correction is to deliver travel direction explicitly, not '
              'to weaken either requirement.')
        return 0
    print('NOT DEMONSTRATED. This evidence has expired; do not inherit the '
          'harness correction it justified.')
    return 1


if __name__ == '__main__':
    sys.exit(main())
