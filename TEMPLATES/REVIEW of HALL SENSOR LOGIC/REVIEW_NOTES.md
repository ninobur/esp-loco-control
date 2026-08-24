# Hall Sensor Logic and Navigation Review

## Review status

- This entire effort is investigatory.
- Nothing recorded in these notes is approved.
- No note, finding, proposal, disposition, or discussion grants implementation approval.
- The review will make provisional keep/change/remove decisions for a future `TEMPLATES` sketch based on QUORUM, so the reviewed design does not have to be reconstructed later.
- Dispositions are operator decisions. Codex explains, checks consequences, and recommends, but does not assign a disposition unless the operator decides it.

## Finding categories

- **Incidental:** observed during review but outside the present navigation question.
- **Interesting:** potentially relevant and worth retaining, with no chosen action.
- **Backup plan:** an alternative if the intended direction is not workable.
- **Intended direction subject to review:** the present direction for investigation, still requiring review and explicit approval.

## Operator note: IR operating conditions

- IR is currently under development because it is not reliable in bright sunlight.
- IR works well in shade or darkness.
- Possible fallback: permit IR-supported automatic operations only when conditions make IR operable.
- Finding category: **Backup plan**.
- Status: operator observation and fallback proposal; not implementation approval.
- Disposition: needs evidence and later review against Templates.
- Exact approval status: unapproved; investigatory only.

## Otto config lines 39–45: IR enable and sensor address

- Code location: `firmware/QUORUM/LL_LocoConfig_9950011.h:39-45`
- Behavior: enables IR Test A unless already defined and supplies the paired IR sensor MAC address unless already defined.
- Interaction with navigation: IR-related; no Hall target-identification function demonstrated here.
- Finding category: **Incidental**.
- Assessment: these lines activate an experimental IR subsystem and bind Otto to a specific sensor. They perform no Hall target identification here.
- Disposition: **Possible deletion from TEMPLATES**. Retain only if the later navigation review assigns IR a demonstrated target-identification or approved operational role.
- Exact approval status: unapproved; investigatory only.

## Otto config lines 46–50: Blynk identity and authentication

- Code location: `firmware/QUORUM/LL_LocoConfig_9950011.h:46-50`
- Behavior: identifies Otto's Blynk control template and selects Otto's authentication token.
- Interaction with navigation: none demonstrated; this supports operator control.
- Finding category: **Intended direction subject to review**.
- Disposition: **Keep in TEMPLATES** for manual-control capability.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 51–58: Blynk virtual pins

- Behavior: maps manual controls and electrical displays to Blynk virtual pins.
- Navigation role: none; operator-control interface.
- Disposition: **Keep in TEMPLATES**.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 60–66: motor-control hardware

- Behavior: identifies motor direction/PWM pins and PWM hardware settings.
- Navigation role: none; necessary motor infrastructure.
- Disposition: **Keep in TEMPLATES**.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 68–74: CTO3 consist extents

- Behavior: describes occupied track ahead of and behind the Hall sensor for multi-locomotive coordination.
- Navigation role: not target identification.
- Finding category: **Incidental**.
- Assessment: marker units intentionally match CTO3's navigation frame. Available physical clearance makes the fixed values acceptable in current practice.
- Disposition: **Keep in TEMPLATES as-is**. Reconsider only if the main sketch or traffic mechanism changes.
- Dependency note: QUORUM references these constants in five CTO3 code locations; they must remain while CTO3 remains.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 76–80: direction values and safe change threshold

- Behavior: defines forward, neutral and reverse values plus the PWM threshold for a safe direction change.
- Navigation role: direction is an input to navigation; safe direction changing is motor protection.
- Disposition: **Keep in TEMPLATES**; values confirmed correct.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 82–83: normal speed constant

- Behavior: defines profile-level `NORMAL_PWM` as 110.
- Navigation role: none demonstrated.
- Disposition: **Delete from TEMPLATES config**; speed is controlled in the main sketch.
- Dependency check: no current QUORUM use found outside the profile definitions.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 85–87: ramp behavior

- Behavior: defines profile-level motor ramp timing.
- Navigation role: none; motor behavior.
- Finding category: **Interesting**.
- Disposition: **Change: reactivate the profile constants in TEMPLATES**, with desired values established during later motor-control review.
- Current behavior: QUORUM manual throttle uses the main-sketch `NORMAL_STEP_MS` value of 150 ms for both increasing and decreasing PWM. These profile constants are unused.
- Intended direction subject to review: locomotive-specific ramp tuning should be visible and controlled in the locomotive config rather than hidden in the main sketch.
- Red flag: `NORMAL_STEP_MS` is also used by automatic-operation paths. Replacing it globally with profile values could change automatic acceleration, braking, stopping distance, station behavior, and traffic behavior. The implementation must explicitly decide whether the profile constants govern manual ramps only or all normal ramps.
- Finding category: **Intended direction subject to review** — ramp behavior should be locomotive-specific and configured in the locomotive profile.
- Scope: recorded for later motor-control review; not part of the present Hall/navigation review.
- Exact approval status: current values not approved; investigatory only.

## Otto config lines 89–97: voltage thresholds and reporting intervals

- Behavior: defines disconnected, limiting, shutdown and recovery voltage thresholds plus counting and reporting settings.
- Navigation role: none; electrical protection and reporting.
- Disposition: **Keep in TEMPLATES**; previously reviewed and confirmed.
- Exact approval status: provisional design decision; implementation remains unapproved.

## Otto config lines 100–166: Hall amplitude thresholds

- Plain behavior: `HALL_DEADBAND_COUNTS` and `HALL_ENTRY_MARGIN_COUNTS` create symmetric opening thresholds around the moving baseline. An excursion of at least 70 counts opens an event (`25 + 45`). Returning within 25 counts of baseline begins event closing. `HALL_MIN_PEAK_DELTA` does not participate in detection or rejection.
- Inputs: averaged Hall ADC reading and calculated baseline.
- Output: event opened/not opened; the event later carries opening polarity, peak and duration.
- Navigation interaction: this is event extraction, not identification of the next physically possible marker.
- Assumption: amplitude can separate genuine markers from unwanted responses with a stable gap.
- Red flag: threshold tuning inherently trades false inclusion against false exclusion. It cannot determine whether a response is the expected route marker.
- Evidence concern: the labels “genuine” and “spurious” are conclusions whose independent ground truth is not stated here. The claimed weak physical track section is an inference from position loss and QUORUM behavior; rejected events were not logged, so the comments do not directly demonstrate weak magnets at those locations.
- Internal contradiction: one session suggested a 91-to-104 count gap; the same evening a 90-count gate reportedly rejected four genuine events. The separation was not stable enough to support a universal classifier.
- Templates comparison: amplitude/flux may contribute evidence to a marker template, but a single global entry threshold should not be treated as target identification.
- Finding category: **Interesting**.
- Codex recommendation for discussion: retain some defensible event-opening threshold for signal acquisition, but do not treat threshold tuning as target identification. Reassess numerical values using independently identified responses.
- Disposition: **Undecided — discussion required**.
- Exact approval status: unapproved; investigatory only.
