# 0050 — The map is deterministic, so the metric is detection, not discrimination

**Date:** 2026-08-28
**Status:** Accepted
**Supersedes the framing of:** [0049](0049-waveform-shape-does-not-identify-a-magnet-on-this-railway.md) (its measurements stand)

## The operator's reframe

> "A Metrolink train going from Newhall to Los Angeles must pass through the
> Newhall Tunnel and Sylmar, in that order. The train can't take Sierra Highway
> or I5 south and bypass the tunnel. Either the train passes through the tunnel
> or it does not get to Sylmar. Deterministically."

This is correct and it changes what should be measured.

NAVI never has to tell one magnet from another. There is no siding that skips
MM101 on the way to MM102. If NAVI **misses no magnet** and **admits nothing
that is not a magnet**, the order does the identifying by itself. The whole
2026-08-28 fingerprint effort was aimed at discrimination — a property the
railway's geometry already supplies for free.

## What the right metric says

12.1 laps on `QUORUM_1_13W`, PWM 90, consist attached, both directions:

| | |
|---|---|
| clean single advances | **2064** |
| magnets **missed** | **1** |
| duplicate detections **absorbed** | 21 — `navMm` never moved |
| reversal artefacts | 1 |

**One miss in 2064 crossings**, and it was not a cruise failure: MM110 → MM114
at 14:38, a 28.6 s gap at PWM 51, during the deliberate slow running at the end
of the session.

**No false accept ever moved the position.** All 21 duplicates were a second
read of the same magnet; every one was absorbed. The failure mode that would
break a deterministic map — a phantom advancing the count — did not occur once.

## Detection margin

```
median peak      190  = 5.4x the min_peak floor of 35
5th percentile   153  = 4.4x
under 2x floor    14 crossings (0.81% of 1736)
```

The railway is not running near its threshold. And the eleven markers that ever
read under 3x the floor — MM004, 012, 061, 063, 064, 077, 080, 081, 109, 144 —
are **the same markers as the duplicate detections**. They are not weak magnets;
they are the trailing half of a split read, already absorbed.

## The "flavour" question, answered

The operator asked whether knowing the magnet type sharpens the description of
the target enough to help. Measured: **it does not.**

| | disk | rectangular |
|---|---|---|
| median peak | 194 | 174 |
| weakest read | 38 | 38 |
| duplicates | 1 in 122 | 1 in 102 |

Both families are detected equally well and split equally often. The three
magnets missed at 14:38 (MM111-113) were all **disk**, the stronger family.
Family membership predicts neither a miss nor a duplicate.

It also cannot add to a *calibrated* marker's expectation, because the
per-marker tables ([0048](0048-expectation-tables-are-per-direction-because-the-railway-has-grades.md))
already describe each magnet individually — a family average is strictly coarser
than a per-marker value. The flavour is real (4.9 sigma, and it identifies the
two magnet types correctly), it is simply already contained in what we have.

## Consequences

- **The per-crossing metric is miss rate and false-advance rate.** Not
  discriminability. Report those.
- Current standing: **1 miss / 2064 at cruise, 0 false advances.** A 171-magnet
  circuit without error is not a hope; it is roughly what the railway already
  does, and the single miss occurred outside the operating regime.
- **The duplicate absorber is load-bearing** and should be recorded as such. It
  fired 21 times in 12 laps. Every one of those, admitted, would have been a
  position error on a deterministic map.
- Worth watching next: whether the one miss reproduces under slow running. A
  28.6 s gap at PWM 51 is exactly where a maximum-time rule has no traction
  (the operator's own rule: no minimum speed means no maximum time except under
  steady running).
- The waveform capture in 1.13W stays. It cost nothing, it is proven inert, and
  it is now the record of what the magnets look like — useful for spotting a
  magnet that *changes*, which is how MM128 was caught.
