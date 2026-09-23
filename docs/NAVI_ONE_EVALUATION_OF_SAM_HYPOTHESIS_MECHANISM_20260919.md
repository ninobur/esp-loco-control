# NAVI_ONE — evaluation of SAM's hypothesis mechanism

**Date:** 2026-09-19  
**Status:** Analysis and shadow prototype. No locomotive-control firmware was changed.

## Verdict

SAM's responses are grounded in the NAVI_ONE field record and describe the
right missing mechanism: observations must feed a bounded set of route
hypotheses rather than irreversibly incrementing one location counter.

The proposal is strongest where it separates three authorities:

1. Hall reports what it observed.
2. railway physics eliminates impossible interpretations;
3. observation history resolves location.

That separation directly addresses delayed strikes after a refused, false, or
wrong-polarity passage.

## Necessary qualifications

### Reachability is one-sided without independent motion

Mapped distance, elapsed time, and a measured physical maximum speed can prove
that an event arrived too soon. They cannot prove that a sufficiently late
event is the expected marker. PWM is commanded effort, not motion, and recent
speed calculated from Hall intervals depends on the observations being judged.

SAM's example eliminates an alternative because its timing is "no longer
consistent with recent motion." That is soft evidence unless an independent
distance sensor supplies the motion measurement. A Hall-only safety boundary
may reject physically impossible speed; it must not impose an unmeasured
minimum speed or maximum arrival time.

### Not every false event creates observable ambiguity

Opening/window disagreement, wrong polarity, stationary detection, or an
impossible interval gives the tracker a reason to fork. A false event that is
physically plausible and has the expected polarity is observationally
identical to the real next marker. A magnet-only system cannot promise to
detect that case immediately.

Route sequence may expose it later. Independent motion evidence would expose
more cases earlier.

### Hypotheses need an explicit fault model and bound

"Perhaps two" hypotheses is not by itself an algorithm. The implementation
must define which alternatives are created, when they are removed, and what
happens after the supported fault bound is exceeded.

The first prototype permits one anomaly since the last unambiguous state:
wrong polarity, false observation, or one missed marker. A hypothesis that
would require a second anomaly is removed. No survivors means LOST and must
eventually cause a controlled stop in a field integration.

### Station control cannot consume an ambiguous best guess

SAM correctly notes that station behavior needs separate design. A production
integration may perform an action only when every surviving hypothesis agrees
that the action is safe. The shadow sketch has no motor or station authority
for this reason.

## Prototype

`firmware/test-programs/NAVI_HYPOTHESIS_SHADOW` implements the bounded core and
a serial replay sketch. It preserves opening/window evidence, applies only a
hard maximum-speed reachability bound, prevents stationary advances, and
retains one-fault alternatives until later route polarity removes them.

The host test covers clean running, too-soon false observations, stationary
events, wrong polarity, missed markers, and opening/window disagreement.

## Next evidence gate

The right next step is not a field flight. It is a converter that feeds the
preserved F02/F03, F05-F07, F08/F09, F13, MM136, MM117, and Grillers records
into this observation format. The prototype passes only if it either maintains
the correct route hypothesis or reports ambiguity/loss before issuing a false
station-safe position.

Only after that replay should the tracker run as a telemetry-only shadow beside
the current navigator. Motor and station authority remain out of scope until
the shadow record establishes both recovery and bounded false ambiguity.
