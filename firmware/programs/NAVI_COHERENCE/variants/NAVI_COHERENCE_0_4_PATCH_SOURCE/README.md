# NAVI_COHERENCE_0_4

Telemetry-format safety correction to NAVI_COHERENCE_0_3 for Toby (9950012).

This is the fourth packaged NAVI_COHERENCE firmware artifact. 0.3 is preserved unchanged as the reviewed predecessor. 0.4 changes **no navigation decision logic**.

## Why 0.4 exists

Independent review of 0.3 found an inherited `snprintf` format/argument mismatch in both `publishNav()` and `publishDecision()`.

Both functions already passed `evidenceClassName(...)` as an argument, but the JSON format strings lacked the corresponding `%s` field. Every later argument was therefore interpreted against the wrong conversion specifier. In `publishDecision()`, which runs for every judged Hall event, integer/timestamp values could be consumed as C-string pointers. Claude reproduced the same argument pattern on the host under AddressSanitizer and obtained an immediate segmentation fault.

This is a pre-flash blocker.

## Changes from 0.3

1. `publishNav()` now includes `"evidence":"%s"` immediately after `"ruling":"%s"`.
2. `publishDecision()` now includes `"evidence":"%s"` immediately after `"ruling":"%s"`.
3. Sketch identity advances sequentially to `NAVI_COHERENCE_0_4`.

There are **no changes** to Navigator logic, Hall acquisition, IR eligibility, mapped history, stop/reversal behavior, rolling DNA, AUTO gating, or evidence authority.

## Navigation behavior unchanged

- NAVI alone controls MM advancement.
- OPERATOR + DIRECTION + MAP are sufficient initial authority.
- Evidence sources are **OPERATOR · DIRECTION · MAP · HISTORY · IR · HALL · TIMING · PWM · STATION_MARKER**.
- A magnet is a declared point, not a magnetic-field zone.
- Valid IR uses the deliberately tight ±10% per-interval experimental eligibility window, reset at each declared MM.
- IR unavailable falls back to the 500 ms timing/recent-motion gate.
- There is no ungated Hall-advance path.
- Hall before a physically possible next landmark is `NON_LANDMARK_HALL`.
- Wrong polarity at the expected physical location is diagnostic rather than a position crisis.
- Missed landmarks preserve continuity and mapped-history alignment.
- Stop preserves position.
- Reversal preserves interval authority/history and starts a fresh unsigned-IR frame.
- Rolling DNA records mapped landmarks passed, not Hall callback count.
- Unresolved position cannot drive position-dependent AUTO/station logic.
- STATION_MARKER remains a reserved optional evidence source.

## Required verification before flash

Run a **clean** independent Arduino build with warnings enabled so build cache cannot hide format warnings.

Verify:

- no `-Wformat` warnings from NAVI_COHERENCE;
- boot reports `NAVI_COHERENCE_0_4`;
- MQTT client ID begins `NAVI_COHERENCE_`;
- the first judged Hall event publishes valid `nav/discrepancy` JSON without reset/crash;
- `nav/marker` / `nav` JSON contains the `evidence` field correctly;
- the focused host navigation suite remains passing;
- Manual only for the first field run.

## Normal model

**KNOWN MM → EXPECT NEXT MM → MEASURE PROGRESS → CONFIRM ARRIVAL → NAVI ADVANCES → REPEAT**

> I know where I was. The track tells me what comes next. Movement tells me how far I have gone. Hall confirms that I arrived. NAVI alone decides when I advance.
