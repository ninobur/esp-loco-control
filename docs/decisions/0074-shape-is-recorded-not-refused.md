# 0074 — Shape is recorded, not refused

**Date:** 2026-09-03
**Status:** **Superseded by 0080, 2026-09-10.** The diagnostic-only principle is
affirmed there; this proposal's retained 0070 stitching exception is not.
**Touches:** 0054 (four conjunctive characteristics), 0062 (the shape test
abstains on a railed passage), 0070 (a passage may not span a stop), 0072
(every judged passage is kept), and the stop-episode rule of X5 (commit
346b577, which has no decision record of its own).
**Evidence:** `docs/NAVI_ONE_1_0X11_EPIPHANY_FIRST_RAILWAY_RUN_ARCHES_CW_20260903.md`
(the Arches CW shutdown); the `NAVI_ONE_STATION_CURVES_0_1` diagnostic run of
2026-09-03 (1,177 passages, every one published); the recognizer's own
tuning table at `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h:24`.
**Builds:** NAVI_ONE_1_0X13_FIELDTEST and NAVI_ONE_STATION_CURVES_0_3, compiled 2026-09-03 night.
**Field:** recorder 0.3 flown 2026-09-04, two hours both directions, 3,284 magnets, 76 stops, no shutdown, no strike; zero passages the old rule would have refused, so the rule's cost is not yet measured (`docs/NAVI_ONE_X13_FIELD_VERDICT_20260904.md`). Still PROPOSED.

---

## The question this record answers

The operator asked, after the diagnostic run, whether curve comparison is
buying anything at low speed. He then asked where the project is, having
watched each gain add a layer of logic that solved part of a problem and
created one of its own.

## The decision proposed, in the operator's words (2026-09-03 night)

> Gaussian shape and two-sided archaeology are diagnostic measurements, not
> navigation tests. They may be calculated, published and archived, but may
> not reject a passage or stop the locomotive.

Navigation admission rests on the existing physical evidence: the capture's
duration floor, the rebound/spacing guard, the amplitude floor, and the
expected polarity and route sequence. Applied globally, not only at
stations. The residual, `shape_tested`, `two_sided`, `trunk` and `why2` stay
on the wire and in the archive, joined by `shape_refuse` / `shape_outcome`
(`would_shape_refuse` on the recorder) so the archive records when the
former rule would have refused. A failed fit, an excessive residual or an
archaeological WRONG_SHAPE / INSUFFICIENT cannot change `isMagnet` once the
physical tests have passed.

That single change retires, because they have nothing left to act on:

1. **The WRONG_SHAPE refusal**, `MagnetRecognizer.h:259`, residual above the
   0.13 ceiling.
2. **The stop-episode rule** from X5, `NAVI_ONE.ino:1367`: any refusal on a
   passage touched by a stop is routed to a navigation shutdown as if it were
   a stitched refusal. Without a shape refusal there is nothing to route.
3. **STITCHED_REFUSED as a shutdown**, and with it the wording fault that
   labelled yesterday's ordinary refusal as stitched.
4. **The rescue half of the archaeology** in `TwoSided.h`: the two-sided
   judgement and "one arc, restored" exist to carry a stop passage past the
   ceiling. Their measurements stay in the record as data; their power to
   change a verdict goes.

The discard-branch fix that was item 5 here is now its own record, 0075, at
the operator's direction that the two decisions be kept separate. It is
summarised for context:

5. **The discard branch amputates flanks** (moved to 0075). `HallCapture.h:363` discards a
   just-opened passage as a false start when the reading looks flat and has
   shown no progress. The flatness test drops the one rising sample from a
   sixteen-sample window, so a genuine flank reads flat for one 25 ms step,
   is discarded, reopened on the next sample, and discarded again; the
   pre-roll ring is not refreshed while this cycles. The surviving record
   begins mid-flank glued to stale pre-roll. The fix is one added condition:
   discard only when the current reading is itself back within the entry
   margin. A reading still climbing is not a false start.

## What stays, and why

| stays | why |
|---|---|
| 0054's amplitude, polarity and spacing tests | They are the tests that have never been wrong in the field. |
| ~~0070 and the stitch across a stop~~ | ~~A magnet the train stops on must be counted once, not twice. That is a counting question and is untouched by shape.~~ **Superseded by 0080: morphology may not gain navigation authority through acquisition or stitching.** |
| 0071, 0073 | Acquisition rules. They fixed real sensor faults and are proven over 1,263 railway passages with zero disagreements and zero floor rejections. |
| 0072, the archive | It is the instrument that let the diagnostic run answer a question instead of raising one. |
| The residual itself, on the wire and in the database | It is still the best single measure of record quality, which is exactly the job it is being moved to. |

## What forced it

**The diagnostic run.** 1,177 passages, 344 of them at station phases, every
station passage accepted. The shape test refused nothing at any station
phase. Station residuals are not worse than cruise (approach and zone median
0.0735 against cruise 0.0691). But the near-misses all sit at the slow
phases: zero ramp maximum 0.1150, departure maximum 0.1180, against a 0.13
ceiling. Cruise maximum was 0.0990. The test does no work at stations and
only threatens the train there.

**The Arches CW shutdown of the same morning.** Two consecutive departure
passages with amputated flanks: MM110 at 0.1139 accepted, MM111 at 0.1301
refused, navigation unset, train stopped. The recording was defective; the
magnet was real; amplitude and pole both said so.

**The history since X5.** Every navigation shutdown since 2026-09-02 has been
a shape refusal of a passage recorded during a stop, and every one examined
was a defect of the recording, not of the magnet. Each fix has been a layer
above the previous one: the ceiling refused stop passages; the stop-episode
rule turned those refusals into shutdowns; stitching and the archaeology
were built to rescue stop passages from the ceiling; the discard branch, added
for a stop on a magnet, now manufactures the shapes the ceiling refuses. Each
layer defends against a failure the layer below created.

## The evidence that arrived after this was written, and what it changes

The recorder run of 2026-09-03 ended in a shutdown on the fourth Arches CW
departure: passage 1805, MM110 targeting MM111, pole N expected and observed,
ratio 1.005, an ordinary passage, residual 0.1372, WRONG_SHAPE, routed to a
shutdown by the stop-episode rule. The raw record shows a 103-count step from
the pre-roll to the first kept sample. All six captured Arches CW departures
show the same step at 35 to 103 counts, with the residual tracking the size
of the hole (`docs/NAVI_ONE_DISCARD_FLANK_FIX_20260903.md`).

The operator's reading: the defect is repeatable, lives in the recorder as
well as X11, and the Gaussian crosses its ceiling only when the amputation is
severe. Making shape advisory, as this record proposes, would have prevented
that shutdown **and masked the corrupted acquisition instead of repairing it.**
That is the reason this record is deferred rather than acted on. The shape
test was, in these six cases, the only thing that noticed the recording was
wrong. Whatever is decided about its authority is decided on records that are
whole.

## The one observed counter-example, found by the replays

Finding 09 A (2026-09-01, NAVI_ONE 0.5): a latched baseline offset held the
reading about 40 counts up for 12 s while Toby stood still; a blip at the end
gave the passage a judged peak of 73, ratio 0.36–0.38 against the 0.34
floor, residual 0.27. Shape refused it, and nothing else would have. Under
this decision it is admitted, navigation advances one marker wrongly, and
the next passage, opposite pole, strikes one marker later. So the residual
HAS uniquely excluded one observed non-magnet. Its cause was an acquisition
latch (finding 08, closed in gate 8 for the moving case; at rest the latch
remains). The operator's rule for this record is that the answer to an
acquisition fault belongs in acquisition, not in a shape veto; the case is
carried in gate 12 as a registered risk with its numbers, and the
experiment's ending condition (a DISAGREE or CONTRADICTED with
`shape_refuse:1`) is written for exactly this shape of event.

The synthetic step, shoulder and double lobe of gate 12 E, which clear
amplitude, time and pole by construction, are now admitted. None has been
observed on the railway. They are a stated theoretical cost, not a hidden
failure.

## The counter-evidence, stated plainly

The shape test was not built on nothing. The recognizer's tuning table
records real passages at residual 0.047 to 0.081 and non-primary false
events at 0.195 to 1.166, from the 2026-08-28 survey of 351 waveforms. Shape
separates those populations cleanly.

But so does amplitude, and 0054 records it: false events top out at ratio
0.26, real magnets start at 0.63. In the survey population the shape test
never cast a deciding vote, because amplitude had already refused everything
shape would have refused. Since then, shape's only deciding votes have been
against real magnets.

The one thing neither run can show is an impostor near a station: a
non-magnet excursion large enough to pass amplitude, of the right pole, at a
possible spacing, that shape alone would have caught. No such event has been
recorded on this railway. If one occurs, the archive will hold its shape and
this decision is reopened with evidence.

## The unintended consequences to name now

- **The recognizer is one vote thinner.** 0054 chose four conjunctive tests
  so that no single test carried the answer. This record reduces the deciding
  tests to three and moves the fourth to advisory. The populations 0054
  measured do not overlap on amplitude alone, but that was measured at
  cruise, on the 2026-08-28 survey. Station-phase amplitude ratios are in the
  diagnostic run's archive and should be examined before this is built.
- **Something that used to stop the train will now let it run.** A refusal
  that was wrong nine times may be right the tenth. The mitigation is that
  every passage is published and archived (0072), so a wrongly accepted
  passage is findable afterwards; but it is found afterwards, not prevented.
- **Retiring the stop-episode rule retires a rule the operator never
  ratified.** It arrived in X5 without a record. That is being said here so
  it is not retired silently either.
- **The archaeology's measurements lose their consumer.** `two_sided`,
  `trunk` and `why2` stay on the wire for the archive. Nobody in the
  firmware acts on them. That is deliberate, and it should not later be read
  as a dead code path nobody decided about.
- **The discard fix changes what is recorded, not what is judged.** Records
  will start earlier and be longer by up to one 25 ms step of flank. Residuals
  on future stop passages will be lower than on the archived ones from
  before the fix. Comparisons across the fix must carry the build, which
  0072 already requires.

## How it would be proven, if approved

1. A gate in the HallCapture harness that drives a synthetic flank of known
   slope through the armed discard branch and asserts the record begins on
   the pre-roll, not mid-flank.
2. The Arches CW MM110 and MM111 records replayed through the recognizer
   with shape advisory: both accepted on amplitude and pole.
3. The 2026-08-28 survey and the diagnostic run replayed with shape
   advisory: no false event accepted that the fielded recognizer refused.
4. One railway run on the resulting build, published every passage, with
   the archive comparing verdicts to X11's on the same magnets.

None of this is built. The build, when there is one, names itself as a
field test.

## Attribution

The operator asked whether curve comparison was buying anything at low
speed and asked where the process stands. That shape should decide nothing,
and which rules fall when it does not, is an agent's answer to those
questions; the retirements are proposed to him, and none is his ruling until
he says so.
