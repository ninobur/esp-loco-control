# Field finding 15 — the boot record is truncated, and the build goes missing

**Date:** 2026-09-03
**Locomotive:** Toby (9950012)
**Build:** `NAVI_ONE_1_0X11_FIELDTEST` ("Epiphany")
**Status:** FOUND, NOT FIXED. Firmware not modified; nothing flashed.
**Found by:** the curve-database ingest, which records every line it cannot
decode instead of dropping it.
**Same fault as:** decision 0071's note that a truncated marker event is a lost
event, because the Pi's `json.loads` throws and the whole line goes.

---

## What happens

`state/bootid` is built in `setup()` with

```c
char b[400];
snprintf(b, sizeof(b), "{\"sketch\":\"%s\",\"subtitle\":\"%s\", ... }", ...);
```

X11 added `"subtitle":"Epiphany"` to a payload that was already 391 bytes. The
result is 399 bytes and a NUL: `snprintf` truncated it mid-string. Every X11
boot publishes a boot record that is not valid JSON.

```
{"sketch":"NAVI_ONE_1_0X11_FIELDTEST","subtitle":"Epiphany", ... ,"resume_max
```

## What it cost

| build | bootid bytes | parses |
|---|---:|---|
| NAVI_ONE 0.9 | 210 | yes |
| NAVI_ONE 1.0X to 1.0X9 | 390–391 | yes |
| **NAVI_ONE 1.0X11** | **399** | **no** |
| NAVI_ONE_STATION_CURVES_0_1 | 428 | yes — the buffer was enlarged in that sketch |

Four boots on 2026-09-03 published an unparseable boot record. Anything that
reads bootid with `json.loads` — the dashboard, any analysis tool, the first
draft of the curve decoder — silently learned nothing about those boots: not
the sketch, not the subtitle, not `field_accepted`, not the capture constants.

The Epiphany bench test of the previous night, which decision 0073 rests on,
ran on X11. Its records are attributable only because the ingest now salvages
the leading fields.

## Why it was invisible

Nothing on the locomotive is wrong. `snprintf` truncates rather than
overflowing, so there is no crash, no warning, and the console `[BOOT]` line
prints correctly because it is a separate `printf`. The only symptom is a
consumer that quietly knows less than it should — and the first draft of
`decode_curves.py` had exactly the `except Exception: return` that made it
invisible.

## What was done

Nothing to the firmware. The decoder now:

- salvages the complete leading key/value pairs from a truncated boot record,
  so the build and subtitle survive;
- writes a row in `rejects` naming the topic, the byte count and the parse
  error;
- flags the boot `bootid_truncated`, which `decode_curves.py audit` counts.

## What needs the operator's hand

The buffer. `char b[400]` wants to be `char b[512]`, the way `publishNav`'s was
raised to 640 for the same reason on 2026-09-02 — the archaeology's `why2`
pushed the marker event past 500 and the comment there already says a truncated
event is a lost event. It is a one-line change to `NAVI_ONE.ino` in a build
that is currently fielded, and it is not mine to make.

Worth considering with it: a boot record is the one message whose loss cannot
be recovered from any other, and it is published exactly once. If the payload
is going to keep growing, it may want to be two messages, or to carry the
identifying fields first and the capture constants second, so that a future
overflow costs the constants rather than the identity.
