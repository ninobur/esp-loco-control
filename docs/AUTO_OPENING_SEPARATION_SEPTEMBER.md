# How long after opening must Otto watch to reject every impostor he actually saw in AUTO?

**September 2026, all builds, AUTO operation only. Analysis. No firmware changed,
no threshold proposed.**

## The short answer

**There is no such number.** No count of additional samples separates 100% of
the September AUTO impostors from 100% of the genuine passages — not at 1 ms,
not at 60 ms, not anywhere in between. The reason is not that the signal is
subtle. It is that **two of the five AUTO impostors are re-reads of a magnet
that had already been counted**, and a re-read is a real magnet's waveform.
There is no information in it to find.

Split the impostors by kind and the answerable part has a clean answer:

| impostor kind | n | earliest k with 100% separation |
| --- | --- | --- |
| **non-magnet openings** (station dwell) | 3 | **15 samples = 15 ms** |
| **re-reads of a magnet already counted** | 2 | **never** — structurally |

---

## 1. Corpus

146 run files, 2026-09-01 to 2026-09-15, 2.8 GB on 192.168.68.142.

```
737 waveform messages
 -> 436 complete distinct records
 -> 323 undecimated (dec=1; a 1 ms question cannot be asked of the other 113)
 -> 115 during AUTO enrolled AND running
 -> 103 after de-duplication (the same passage appears in several dumps)
```

**Attribution matters here and was wrong on the first pass.** A waveform dump is
triggered by a strike, so the *message* timestamp is when AUTO stopped, while
the five records inside it are passages from seconds earlier, while AUTO was
running. Classifying on message time put every X17/X18 record outside AUTO.
The records are therefore placed by **device uptime** (`openedAtMs` / detection
instant) against the 1 Hz `uptime_ms` in the STATUS alerts, not by wall clock.

Builds contributing genuine AUTO records: X13 (38), X15 (14), X17 (4), X19 (8),
X20 (31). X16 and X18 contributed none — all their dumps land outside AUTO.

**Compatibility.** X13–X18 use the 40-byte record, which begins *at* the
opening sample. X19/X20 use the 57-byte record with 512 ms of pre-roll, so the
opening sample is at index `preSamples`. Both are oriented and both give
"samples from opening onward", which is all this question needs. The opening
criterion differs in anchor (global reference vs local minimum) but not in
magnitude — 70 counts in both.

## 2. Populations

- **GENUINE — 95.** AUTO running, PWM > 0, moving, and the navigator ruled
  `ADVANCED` — the map confirmed the pole.
- **IMPOSTOR — 5.** Openings in AUTO that were not a magnet passage:

| build | when | why | PWM |
| --- | --- | --- | --- |
| X13 | 09-10 01:48:34 | re-read, refused `TOO_SOON` | 90 |
| X15 | 09-13 11:20:59 | re-read, refused `TOO_SOON` | 90 |
| X19 | 09-15 18:58:21 | stationary in the Arches dwell | 0 |
| X19 | 09-15 18:58:43 | stationary in the Arches dwell | 0 |
| X19 | 09-15 18:58:43 | stationary in the Arches dwell | 0 |

- **EXCLUDED — 8.** `WRONG_MAGNET` / `NO_POSITION` in AUTO. These are real
  magnets read against a map pointer that had drifted, not impostor openings.
  Every one of them rises at least as much as the weakest genuine passage
  (max-rise at 15 ms: 9–37, against a genuine floor of 4), so counting them as
  impostors would make separation impossible at any k by construction.

## 3. Sample-by-sample, all five impostors together

Statistics computed only from samples 0..k after the opening sample:

```
  k    rise at k          max-rise so far      100% separated?
       gen-min  imp-max   gen-min  imp-max
   0      0        0         0        0        no
   5     -9       14         0       14        no
  10      0       26         1       26        no
  20      0       35         4       35        no
  30      4       50         7       50        no
  40      5       54        12       54        no
  60    -24       52        14       64        no
```

Never. And the overlap is two-sided, which is why no cleverness rescues it:

- **Genuine magnets that barely move.** X13 2026-09-10 18:21:00, PWM 46,
  peak 84 — rises **14 counts in 60 ms**. X15 2026-09-11 23:44:42, PWM 31,
  peak 84 — rises 15.
- **Impostors that rise like magnets.** X15 2026-09-13 11:20:59 traced from its
  opening, every 5 ms: `48 54 62 78 83 90 98 100 102 101 101 109 100`. That is a
  textbook arc, because it is one — the same magnet, read a second time.

## 4. Non-magnet openings alone (the three Arches dwell events)

Statistic: **value at k minus value at the opening sample.**

```
   k    genuine min   impostor max    margin
  12         +4           +28         OVERLAP
  13         +4           +28         OVERLAP
  14         +4           +27         OVERLAP
  15         +4           -17           +21     <-- earliest
  16         +4           -47           +51
  20          0           -61           +61
  30         +4           -51           +55
  40         +5           -56           +61
  60        -24           -73           +49
```

**Earliest k = 15 samples (15 ms).** It fails at 12, 13 and 14 and then
separates **continuously at every k from 15 to 60** — margin minimum 21,
median 55, maximum 67. This is not one noisy sample.

**The two records that set the boundary at k=15:**

- **Worst genuine, +4.** X15 2026-09-11 23:44:42, PWM 31, peak 84. Traced from
  opening every 3 ms: `66 68 67 68 70 70`. A slow weak passage that has barely
  begun to rise.
- **Worst impostor, −17.** X19 2026-09-15 18:58:43, PWM 0, peak 101, Arches
  dwell: `101 79 69 69 85 84`. It opens at its own maximum and sags.

The other two dwell impostors are −52 and −85 at k=15. The third
(`84 97 100 109 112 32`) is the interesting one: it rises like a magnet for
14 ms and *then* collapses — which is exactly why 14 is not enough and 15 is.

## 5. What this does not establish

- **The binding limit is the impostor count, not the sample count.** Three
  non-magnet openings and two re-reads. A 15 ms boundary resting on three
  records is a measurement, not a guarantee.
- **The genuine set is a sample.** Waveforms are dumped only on refusal, on a
  strike, and for one accepted candidate every 15 s. 95 records stand for tens
  of thousands of September passages, so the weakest genuine magnet of the month
  is probably weaker than the weakest one here.
- **Impostors that were accepted and never refused or dumped are invisible.**
  The three Arches events are in this corpus only because the strike that
  followed them dumped the trailing window.
- **113 of 436 records are decimated** at 2–8192 ms per sample and cannot
  answer a 1 ms question at all.
- The statistic in §4 is descriptive. It is not proposed as a test or a
  threshold, and nothing in the firmware was changed.

## 6. The plain-terms answer

Once the ≥70-count signal opens, **15 more milliseconds** would have rejected
every *non-magnet* opening Otto saw in AUTO in September without rejecting a
single real magnet — 95 genuine kept, 3 impostors refused, margin 21 counts,
and it holds for the following 45 ms rather than flickering.

But that is not all the impostors. The other two were the same magnet counted
twice, and **no amount of watching after the opening can ever reject those**,
because what is being watched is a genuine magnet. Separating those needs
elapsed time since the previous accepted magnet — which is what the rebound
guard already does — not more samples.
