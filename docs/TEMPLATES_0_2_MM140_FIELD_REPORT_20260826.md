# TEMPLATES 0.2 field report — MM 140 exclusion and delayed omission recovery

**Date:** 2026-08-26  
**Locomotive:** Toby (`9950012`)  
**Sketch:** `TEMPLATES_0_2`, test program based on QUORUM 1.16R  
**Run:** CCW, Auto, nominal PWM 90  
**Stopping location:** Arches, by operator command  
**Evidence status:** field observation plus MQTT record; no raw 1 kHz UDP trace was produced by this build

## 1. Purpose and result

This was the first field exercise of TEMPLATES 0.2 after repair of Toby's DIR wire. TEMPLATES 0.2 changes the admission gate while leaving QUORUM's navigation and omission recovery downstream. Its central proposition is that questionable Hall excursions should be excluded permanently because QUORUM can recover from a missing magnet more safely than it can recover from an admitted false magnet.

The test produced a valuable, repeatable result. TEMPLATES rejected the weak passage at physical MM 140 on two laps. QUORUM eventually inferred an omission and repaired its position on both laps. However, because MM 140 and the following magnets shared South polarity, QUORUM initially named later physical magnets with the wrong coordinates and published several of those identifications as ordinary `AGREE` events. The excessive elapsed interval already showed that a magnet might have been missed, but unchanged QUORUM allowed matching polarity to override that warning.

The admission principle worked: the weak passage never entered navigation. The downstream response was not sufficiently conservative: position became wrong before recovery began.

## 2. Evidence and access

The authoritative full MQTT record remains on the Raspberry Pi:

```text
/home/david/NGR/telemetry/all_20260826.log
```

The segmented run used for this report is:

```text
/home/david/NGR/telemetry/runs/9950012_20260826_231627.log
```

Claude, Codex, or another reviewer can inspect it with:

```bash
ssh david@192.168.68.142 \
  'less /home/david/NGR/telemetry/runs/9950012_20260826_231627.log'
```

Relevant MQTT topics are:

```text
ngr/loco/9950012/diag/admit_reject
ngr/loco/9950012/state/nav
ngr/loco/9950012/mm/marker
ngr/loco/9950012/state/loopstat
ngr/loco/9950012/alert
```

The raw Hall receiver was running, but its capture remained zero bytes. TEMPLATES 0.2 was evidently built without its UDP trace output enabled. Consequently, this report does not claim sample-level waveform evidence. Peak, duration, PWM, timing, admission disposition, navigation decisions, and operator observations are preserved through MQTT.

## 3. Physical inspection

After the run, David inspected the track. The magnets in this section are ballasted. MM 140 is near a sprinkler, and ballast beneath it had been partially washed out. MM 140 is a South-polarity magnet; MM 139 is also South.

The washout is a credible explanation for the repeatably weak passage. The magnet may have dropped, tilted, shifted laterally, or acquired greater clearance from Toby's Hall sensor. This is defective signaling infrastructure. The correct response is to report and repair the marker, not lower the admission threshold to accommodate it.

An unrelated DIR-wire fault occurred before the useful run. It caused forward and reverse commands to produce the same physical motion. The wire was repaired before this run. Evidence from the DIR-wire episode is not evidence about TEMPLATES admission or navigation.

## 4. First repeated episode

The first MM 140 rejection occurred at `23:19:49.265`:

```json
{"cause":"AMPLITUDE","pol":"S","peak":111,"dur_ms":190,"floor_ms":80,"pwm":90,"peak_rejects":2,"floor_rejects":2,"audit_drops":0}
```

The surrounding navigation decisions were:

| Time | Reported result | Observation | Expected | Peak | Interpretation |
|---|---|---:|---:|---:|---|
| 23:19:47.956 | `AGREE`, MM 141 | S | S | 144 | Last confirmed position before the omission |
| 23:19:49.265 | admission rejection | S | — | 111 | Physical MM 140 excluded permanently |
| 23:19:50.577 | `AGREE`, MM 140 | S | S | 158 | Almost certainly physical MM 139, misnamed MM 140 |
| 23:19:51.910 | `AGREE`, MM 139 | S | S | 164 | Almost certainly physical MM 138, misnamed MM 139 |
| 23:19:53.275 | `AGREE`, MM 138 | S | S | 154 | Almost certainly physical MM 137, misnamed MM 138 |
| 23:19:54.656 | `DISAGREE`, MM 137 | N | S | 173 | Later polarity finally exposed the displacement |

Additional disagreements and coincidental agreements followed. QUORUM opened evaluation at reported MM 129, tied twice, and published `QUORUM_ADOPTED` at `23:20:08.733`. It subsequently returned to ordinary agreements.

## 5. Second repeated episode

The same behavior repeated on the next observed lap. The second MM 140 rejection occurred at `23:26:09.586`:

```json
{"cause":"AMPLITUDE","pol":"S","peak":108,"dur_ms":190,"floor_ms":80,"pwm":90,"peak_rejects":6,"floor_rejects":2,"audit_drops":0}
```

The corresponding decisions were:

| Time | Reported result | Observation | Expected | Peak | Interpretation |
|---|---|---:|---:|---:|---|
| 23:26:08.288 | `AGREE`, MM 141 | S | S | 141 | Last confirmed position before the omission |
| 23:26:09.586 | admission rejection | S | — | 108 | Physical MM 140 excluded permanently |
| 23:26:10.910 | `AGREE`, MM 140 | S | S | 155 | Physical MM 139 misnamed MM 140 |
| 23:26:12.262 | `AGREE`, MM 139 | S | S | 162 | Physical MM 138 misnamed MM 139 |
| 23:26:13.631 | `AGREE`, MM 138 | S | S | 154 | Physical MM 137 misnamed MM 138 |
| 23:26:15.026 | `DISAGREE`, MM 137 | N | S | 175 | Displacement becomes visible |

The later scoring pattern was effectively identical to the first lap:

```text
MM 129  QUORUM_OPEN
MM 129  QUORUM_TIED     scores [2,0,3,0,2,2]
MM 128  QUORUM_TIED     scores [3,0,4,1,2,3]
MM 126  QUORUM_ADOPTED  scores [3,1,5,1,3,3]
MM 125  AGREE
         QUORUM_CLOSED
```

The repeatability makes an accidental electrical spike explanation implausible. The same physical location produced nearly identical rejected peaks and the same downstream navigation sequence on two laps.

## 6. Timing evidence that was available but ignored

On the second lap, the interval from reported MM 141 to the passage subsequently named MM 140 was 2.630 seconds. Neighboring passage intervals were approximately 1.3 seconds. The first lap showed the same effect: the corresponding interval was 2.635 seconds.

Thus the passage named MM 140 arrived after approximately two ordinary marker intervals. Matching South polarity did not prove it was MM 140. The timing evidence made physical MM 139 a live explanation before the passage was named.

The operator saw this as a reporting delay on the dashboard. The MQTT timestamps confirm the observation.

## 7. What TEMPLATES did correctly

1. It rejected MM 140 consistently using the declared admission rule.
2. It gave the rejection no navigation authority. The rejected event did not advance position, enter QUORUM evidence, or return later.
3. It preserved an audit record with cause, polarity, peak, duration, duration floor, PWM, cumulative counters, and audit-drop count.
4. It converted defective signaling infrastructure into an omission rather than accepting a questionable target.
5. The downstream system ultimately found a one-marker explanation and returned to normal navigation.
6. Toby continued to Arches and stopped there under operator control.

These are meaningful successes. The threshold should not be lowered merely to rescue the defective MM 140 signal.

## 8. What the navigation response did incorrectly

1. It treated polarity agreement as sufficient identity even after too much travel time had elapsed.
2. It named the first post-omission passage MM 140 instead of entering missed-magnet uncertainty.
3. It published several wrong coordinates as clean `AGREE` events.
4. Same-polarity adjacency masked the omission until a later North passage appeared.
5. It allowed coincidental agreements to reset the visible miss streak while position remained displaced.
6. It repaired position only after approximately twenty seconds and many passages.
7. During that interval, station, traffic, or other coordinate-dependent actions could have been associated with the wrong physical locations.

Eventual repair is valuable, but it does not make the intervening false certainty acceptable.

## 9. Incorrect interpretations made during the test

The review process also produced mistakes that should remain on the record:

1. Codex initially attributed Toby's reverse movement to physical orientation or session-direction declaration despite David's direct observation that Toby was facing clockwise and both forward and reverse produced backward motion. The actual cause was the DIR wire. Direct operator observations should have been given greater evidentiary weight.
2. The isolated nature of Toby's expected exclusions had previously been described as benign. This test showed that a single isolated exclusion can still create an extended wrong-coordinate interval when the following polarity sequence repeats.
3. Eventual QUORUM recovery was initially described too favorably. The more important fact is that recovery occurred only after several wrong positions had already been published as agreements.
4. Analysis initially concentrated on admission and polarity. It did not immediately recognize that the doubled elapsed interval was already sufficient reason to withhold the MM 140 identity.

These corrections do not undermine the admission doctrine. They clarify what downstream navigation must do with an intentional omission.

## 10. Required response in a successor design

After a last confirmed marker, passage identity must require both credible physical admission and a physically credible arrival interval. Polarity is supporting evidence, not proof of identity.

For this episode, the correct response should have been:

1. Confirm MM 141 normally.
2. Exclude the weak MM 140 excursion permanently and publish its diagnostic record.
3. Continue tracking elapsed time and a physical reachability envelope without resurrecting the excluded waveform.
4. When the next credible South passage arrives after roughly two marker intervals, enter `MISSED_MAGNET_SUSPECTED` rather than naming it MM 140.
5. Keep primary position anchored at the last confirmed coordinate, MM 141. Record the new passage as an observation whose identity is unresolved.
6. Maintain physically viable hypotheses, including that the passage is MM 139 after one omission. Do not treat absence alone as proof of an omission.
7. Use subsequent admitted passages and timing to eliminate alternatives.
8. Adopt a new coordinate only when one explanation is unique under the declared recovery rule.
9. Inhibit coordinate-dependent station or traffic actions while position identity is unresolved.
10. Report the suspected defective marker and later recovery outcome without ever allowing the rejected MM 140 event back into navigation.

This is uncertainty about position, not quarantine of a questionable signal. The signal was rejected and forgotten by navigation. The locomotive moves on while its navigator honestly withholds an unproven name.

## 11. Infrastructure reporting requirement

The system should preserve an operator-facing maintenance report when evidence supports a likely defective marker. At minimum it should include:

- suspected marker number and route direction;
- expected and observed polarity;
- admission-rejection cause;
- peak, duration, threshold, PWM, and elapsed interval;
- previous confirmed marker;
- number of repeated occurrences at the same suspected location;
- whether position became unresolved;
- whether recovery succeeded and what omission was adopted;
- whether any coordinate-dependent action was inhibited;
- audit loss or transport-gap indicators.

For this run, a suitable summary would be:

```text
WEAK_MARKER_SUSPECTED: MM140, South, CCW; peaks 111 and 108 against
140-count admission threshold on two laps; passage excluded; next admitted
passage arrived after approximately two normal intervals; one-marker omission
eventually adopted; inspect marker/ballast near sprinkler.
```

This is a maintenance diagnosis supported by repeated location, timing, map context, and recovery. It must remain observational and must not reinstate the rejected signal.

## 12. Replay and field acceptance tests

A later offline replay must reproduce this exact episode from the MQTT record and, when raw capture is available, from the sample stream. The successor behavior is acceptable only if:

1. Both peak-111 and peak-108 MM 140 passages remain excluded.
2. Rejected events never advance or contribute navigation evidence.
3. The approximately 2.63-second post-MM-141 interval prevents ordinary MM 140 confirmation.
4. The navigator enters an explicit missed-magnet or unresolved-position state before publishing another coordinate.
5. No physical MM 139/138/137 passage is published as a clean MM 140/139/138 agreement.
6. Repeated South polarity does not conceal the unresolved position.
7. Recovery adopts the one-marker omission only after the explanation becomes unique under the specified evidence rule.
8. Coordinate-dependent actions remain inhibited during uncertainty.
9. The maintenance report identifies MM 140 as suspected infrastructure without lowering the admission threshold or resurrecting its signal.
10. Repairing or restoring MM 140 permits ordinary confirmation with no false missed-magnet episode.
11. A deliberately long but physically valid single interval does not automatically become an omission; the system must retain alternatives until evidence distinguishes them.
12. Missing timing, transport gaps, reversal, dwell, or stopping in the interval cannot manufacture a coordinate.

## 13. Field disposition

TEMPLATES 0.2 demonstrated that conservative admission can turn a defective marker into a recoverable omission. It also demonstrated that unchanged QUORUM is not sufficiently conservative after that omission. The test therefore supports the admission doctrine but falsifies the claim that admission cleanup alone is enough for safe coordinate integrity.

MM 140 should be repaired as infrastructure. Firmware thresholds should not be relaxed to accommodate it. Before further unattended or coordinate-dependent TEMPLATES operation, the navigator should enter missed-magnet uncertainty when arrival timing makes the expected identity doubtful, and it must refrain from naming subsequent passages until position is resolved.
