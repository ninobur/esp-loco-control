# Raw Hall evidence package for independent review

This package contains representative, native-resolution Hall passages extracted
from the two specified Pi captures. It is evidence preparation only. It does
not identify Return to Baseline, select a baseline, classify passages, smooth
values, interpolate missing samples, or propose an algorithm.

## Sources

| locomotive | Pi source | capture format | session used |
|---|---|---|---|
| Otto 9950011 | `/home/david/NGR/hall_records/xhr_20260916_191904.xhr` | XHR1 | `C3B93D0B` |
| Toby 9950012 | `/home/david/NGR/qt_logs/qt_20260824_184813.qtcap` | QTRACE01 | `D7651658` |

The complete binaries are included in `sources/` through Git LFS, rather than
as ordinary Git blobs. The source SHA-256 values, session identifiers,
extraction selections, and source event metadata are in `manifest.json`.

The source bundle also includes the synchronized Toby telemetry and receiver
record supplied with the captures, plus the later Toby logs copied into
`field-records/logs/`.

## What is included

`passages/*.csv` contains wide context windows around recorded event anchors.
The windows are deliberately packaging context, not proposed physical
boundaries. The default is 2,000 native samples/milliseconds before and after
each anchor. This value is not inherited from any navigation or RTB rule.

Every row retains the source sample index, native timestamp/delta, raw Hall
value, and synchronized fields available in that format. Otto rows also retain
recorder baseline, PWM, direction, station phase, navigation context, and an
explicit `gap_before` field. Toby rows retain QUORUM's averaged Hall value,
its recorder baseline, peaks, PWM, direction, event-active flag, e-stop and
late flag.

The numerical CSV samples are the evidence. Event polarity, PWM, and recorder
rulings are source metadata, not conclusions supplied by this package.

## Integrity and gaps

The selected Otto session is the complete `C3B93D0B` XHR session: no missing,
duplicate, or reordered sample datagrams were reported by `tools/xhr_decode.py`.
The recorder still reports native measured timing gaps; those values are kept
in `dt_us` and are not repaired. Other XHR sessions in the source file are not
joined to it.

The selected Toby session is kept separate from every other QT session. QT
sample and decision streams have independent transport sequence spaces. The
extractor stops at a truncated final frame rather than inventing samples. Any
source limitations visible in the selected windows remain in the CSV values.

## Reproduction

From the repository root, after placing the two Pi binaries at the paths below
or supplying equivalent local paths:

```bash
python3 field-records/analysis/rtb_evidence_20260926/scripts/extract_passages.py \
  --xhr /private/tmp/ngr_rtb_sources/xhr_20260916_191904.xhr \
  --qt /private/tmp/ngr_rtb_sources/qt_20260824_184813.qtcap \
  --xhr-session C3B93D0B \
  --qt-session D7651658 \
  --outdir /tmp/rtb_evidence_reproduced
```

The extractor uses the repository's XHR format decoder. Its small QTRACE
adapter preserves the existing QUORUM TRACE wire layout from the repository's
`claude/quorum-hall-waveform-diagnostic-plutez` tooling lineage; it only
unpacks records and does not interpret the waveform.

The selected passages are intentionally not a claim that the source recorder's
event anchor is the physical return point. Independent reviewers should use the
raw numerical trace and the synchronized context to determine what, if
anything, constitutes ordinary-track return behavior.
