# TEMPLATES Revision 3 — Target acquisition, not threshold rejection

**Status:** Independent design synthesis for operator and peer review.
**Authority:** Design-only. This document authorizes no firmware change,
build, flash, field operation, or IR control authority.
**Date:** 2026-08-27
**Evidence baseline:** field sessions `9950012_20260827_114203`,
`9950012_20260827_123552`, `9950012_20260827_134856`; offline captures
`toby_ccw_20260824`, `otto_ccw_20260824`, `otto_cw_20260824`,
`otto_change1_20260825`; `firmware/test-programs/TEMPLATES/TEMPLATES.ino`
as flashed 2026-08-27.

## 1. Result first

TEMPLATES' admission gate asks the wrong question. Today it asks "is this
signal strong enough to be a magnet, in general" — one flat amplitude floor
(140 counts) applied identically to all 171 markers, blind to which marker
is next, blind to direction, blind to everything the system already knows.

The system already knows a great deal. Given a starting position and
direction, it knows the expected polarity of the next marker
(`dnaAt`), that marker's expected field strength relative to the railway
median (`strengthAt`, measured, wired, and currently used nowhere upstream
of quarantine), the physical distance to it (`spacingMm`) and therefore,
combined with commanded PWM, roughly when it should arrive
(`dt_expected`, already computed and published today). It does not yet
measure a shape (duration is currently the only shape proxy, and it is
weak alone), and it does not yet consult an independent second sensor
(IR on an unpowered wheel).

Revision 3's governing change: **stop asking "does this clear a universal
floor" and start asking "does this match the specific target we are
already looking for."** A weak-but-expected read (MM141 at 125, against an
expected ~144) is evidence *for* MM141, not noise to discard. A
strong-but-unexpected read is a warning, not automatic confirmation. The
admission decision moves from a one-axis filter to a multi-attribute match
against a target profile the system already possesses before the magnet
arrives.

This is not a new philosophy replacing the existing one. It is the same
asymmetry the current design already enforces — false negatives recoverable,
false positives corrupt the map — applied with more of the available
evidence, so fewer real magnets get treated as if they were noise.

## 2. What today's data actually shows

### 2.1 The admission gate's real behavior in the field

Across three sessions today (CW, CCW, and a live in-progress run), 26
`AMPLITUDE`-caused rejections were recorded. Every one that was not pure
noise (peak <60, duration <30ms — one instance) had a duration comfortably
clearing its floor by 45–70ms at every PWM tested (42, 61, 84, 90, 118).
Duration did zero discriminating work; amplitude alone decided every real
case. This confirms the code's own finding from the original 7,238-excursion
offline capture, now against live field traffic.

### 2.2 The rejected magnets were real, not noise

Two physical zones produced the great majority of today's rejects: MM140–142
(CW and CCW, both directions) and MM69–71 (CCW). At MM140–142 specifically,
peaks clustered tightly at 108–146 across three separate passes at two
different PWM settings (90, 118) — far too consistent, repeatable, and
location-locked to be electrical noise. Physical inspection confirmed
position nominal; the magnets were genuinely weak, not misplaced.

### 2.3 The per-marker strength map already predicts this

`strengthPct[]` (measured 2026-08-20/21, cross-validated Otto/Toby at
r=0.938) gives MM140 = 69% of railway median, MM141 = 76%, MM142 = 76% —
the three weakest-adjacent markers on the entire 171-marker table, and the
same relative ordering (140 weakest, then 141, then 142) that today's field
peaks independently reproduce. Railway median peak runs ~190; MM141's
expected peak is therefore ~144. Today's field reads at MM141 (125–130)
missed that *specific* expectation by ~10–13%, while missing the *flat*
admission floor by 7–11%. The table already knew this marker would read low
before today's session ran.

### 2.4 Duration, PWM-normalized, is a real but weak second vote

Within the marginal peak band (110–149) across the full offline dataset,
raw duration spans 7–1,688ms — no separation. Normalized by PWM
(duration×PWM, a rough proxy for physical magnet width), 74% of that same
population clusters into one band; the remaining 26% is scattered 3–10x
off. Duration-normalized-by-speed is a real corroborating signal, not a
decisive one — it belongs in a fused score, not as an independent gate.

### 2.5 Timing prediction already exists and is unused at admission

`spacingMm[]` plus commanded PWM already produce `dt_expected` and a live
`timing_gate` state (`ACTIVE`/`RAMP`), published on every marker today. The
"how long until we expect to arrive" prediction Revision 3 asks for is not
new engineering — it is already computed, just not consulted at the point
where a candidate event is accepted or discarded.

### 2.6 IR corroboration is close, and is a genuinely independent witness

Amplitude, duration, timing, and PWM-derived speed all depend on the same
Hall sensor and the same motor-PWM chain — a systematic fault (electrical
interference, a mounting issue, a wiring fault) can degrade several of them
together. IR speed sensing on an unpowered wheel shares none of that chain.
The 86-byte IR/Hall fusion-interval packet format already exists and
partially works — 121 of 186 reports decoded cleanly from today's CW
session. Known limitation, accepted as designed: reliability drops in
bright sunlight (documented separately in the IR daylight test series).
Revision 3 treats IR as a corroborating vote that is sometimes unavailable,
not a required input — consistent with the omission-tolerant design already
governing everything else in this system.

## 3. Governing principles (unchanged, restated for this revision)

### 3.1 Error asymmetry still governs

False negatives are recoverable — QUORUM's arbitration, `NO_QUORUM`
self-resolution, and dead reckoning already exist to absorb a missed
marker. False positives corrupt the coordinate system and have no
correction mechanism once admitted. Every change in this revision must
make it *easier* to confirm a real, expected magnet and *no easier* to
admit an unexpected one.

### 3.2 Direction certainty is not position certainty

Direction (`session_dir`) is operator-declared and does not drift.
`navMm` can drift even while direction stays correct — today's own CW
session demonstrated this exactly: three consecutive correctly-rejected
weak magnets left the position counter three markers behind physical
reality, with direction never in doubt for a moment. A target profile
built from a wrong `navMm` is not "no information," it is *confidently
wrong* information, and the design must not let a match against a wrong
hypothesis look identical to a match against a right one.

### 3.3 Identity is conjunctive, and sequence is the deciding conjunct

A single marker's polarity, strength, duration, and timing signature is
not guaranteed unique across 171 markers — a strong composite match at one
marker can still coincidentally resemble another. Uniqueness comes from
matching a *run* of consecutive markers against the DNA map's own
self-similarity bound (the existing `SUFFIX_RESCUE_N=7` finding: no run
shorter than 7 is provably unambiguous against the map itself). A single
high-confidence match should raise confidence; only a matching run should
be allowed to authorize a position correction against the system's current
belief.

### 3.4 Dead reckoning and QUORUM's recovery machinery are preserved, not replaced

This revision changes what happens at the admission boundary. It does not
touch `NO_QUORUM` self-resolution, the arbitration scoring already in
place, or dead reckoning as the fallback when no target match is found.
Those mechanisms are why the system can recover from a missed magnet at
all, and nothing here proposes doing without them.

### 3.5 No match is still the same conservative default as today

A signal that matches no plausible target profile gets exactly the
treatment an amplitude-floor reject gets today: no navigation afterlife,
audit record retained, position unchanged. The bias shifts from "did this
clear a universal bar" to "did this match something we were looking for,"
not away from rejecting the unmatched case.

## 4. The attribute set

Given a target hypothesis (current `navMm`, `navDir`, and the next mapped
marker in that direction), the profile to match against consists of:

1. **Polarity** — `dnaAt(nextMm)`. Already implemented, currently the sole
   input to AGREE/DISAGREE.
2. **Strength** — `strengthAt(nextMm)` scaled by the locomotive's current
   trailing-median gain. Already implemented, currently used only in §3Q
   quarantine, after admission.
3. **Morphology (minimum viable form: amplitude + duration jointly)** —
   requires a new per-marker expected-duration table (`durationAt`,
   analogous construction to `strengthPct[]`: measured, PWM-normalized,
   cross-locomotive validated) to pair with the existing peak. True shape
   (rise/fall symmetry, plateau width) remains unavailable without waveform
   retention, which the current 1.16R base does not have.
4. **Timing** — `spacingMm[nextMm]` and commanded PWM, already computed as
   `dt_expected`. New: feed it into the admission-time score rather than
   only the post-hoc `timing_gate` display.
5. **Sequence** — the run of the last N accepted (or match-scored) markers
   compared against the map's own minimum unambiguous run length. Governs
   how much combined evidence is required before a *correction* to current
   position is authorized (§3.3); a single-marker match is confirmatory
   evidence, not correction authority.
6. **IR corroboration** — independent pulse count and interval, when
   available and confident. Raises or lowers overall score; never gates
   alone, never required.

## 5. What this changes and does not change at the admission boundary

**Changes:** the admission gate gains access to `navMm` and `navDir`
(currently deliberately withheld from it) in order to compute a target
profile before a candidate event is scored. This is the one architectural
line this revision proposes crossing, and it is crossed carefully — see
§6.

**Does not change:** the gate's authority to reject remains absolute when
no profile is matched. The downstream layers (§3Q quarantine, arbitration,
`NO_QUORUM`) are untouched. TEMPLATES remains scoped to the admission
question only.

## 6. The open design question: correction authority

Giving the admission gate a position hypothesis creates the one real risk
this document does not resolve on its own: a wrong hypothesis can produce
a false "match" and entrench itself, which is the same failure category
the whole design exists to prevent, moved one layer earlier.

The mitigation proposed for review, not yet a decision:

- A match against the *current* target hypothesis, at normal confidence,
  behaves as today — it confirms and advances position by one, same as an
  ordinary AGREE.
- A match that would *correct* the currently-held position (i.e., resolve
  in favor of a different `navMm` than the one currently believed) requires
  the sequence-run standard from §3.3 — a matching run at the unambiguous
  length, not a single strong composite score. This is a stricter bar than
  ordinary confirmation, by design, because it is the exact case where a
  wrong hypothesis and a right one are hardest to tell apart from a single
  event.
- Below both thresholds: no navigation afterlife, exactly as an amplitude
  reject behaves today.

This is the one place in the design where "how much evidence is enough"
is a judgment call rather than a measurement, and it is flagged here for
operator decision rather than assumed.

## 7. Honest limits

- **Morphology is amplitude+duration only, by explicit scope decision.**
  True waveform shape is not available without retaining raw samples per
  event, which the current 1.16R base does not do. This revision does not
  propose porting the trace overlay; it proposes using the two scalars
  already available, jointly rather than separately.
- **`durationAt()` does not exist yet.** It is proposed by direct analogy
  to `strengthPct[]`, using the same measurement methodology, but has not
  been built or validated. §2.4's 74%/26% split is evidence it is worth
  building, not evidence of its final accuracy.
- **IR corroboration is not yet operational for this purpose.** The packet
  format decodes; using it as a confidence input to admission has not been
  implemented, tested, or validated against a live capture, and will not
  function in bright sunlight — accepted as designed, per §2.6.
- **The correction-authority threshold in §6 is unset.** A specific
  sequence length, confidence score, and scoring formula are needed before
  this is implementable, and are deliberately left open here rather than
  guessed.
- **No claim of improved precision is made.** Everything in this document
  addresses recall (recovering weak-but-real magnets currently discarded).
  Nothing here has been tested against Otto's contaminated captures, where
  precision — not recall — is the open risk.

## 8. Unresolved operator decisions

1. Does correction authority (§6) require a strict sequence-run match, or
   is a lower, confidence-weighted bar acceptable given QUORUM's existing
   recovery machinery as a backstop?
2. Is building `durationAt()` from the existing offline captures sufficient
   evidence to field-test, or does it need a dedicated measurement pass
   first, the way `strengthPct[]` had one?
3. Should IR corroboration be integrated now (packet format exists,
   decoding partially works) or held until the receiver-coexistence gate
   Codex is building on the car side is complete?
