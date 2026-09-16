# NAVI_ONE_X18_RECORDER — X18 as flown, plus a continuous Hall record

**OBSERVATION BUILD.** X18 exactly as flown on 2026-09-15 (commit `242109e`),
with a one-way tap that streams the complete 1 kHz Hall signal to the railway
Pi over UDP. It records, and it changes nothing.

Acquisition, recognition, navigation, stations, throttle, safety and the map
are **byte-identical** to `242109e`. The recorded data has no authority over
any of them, there is no code path by which it could acquire any, and losing
every record at any time changes nothing the locomotive does.

Full report: `docs/NAVI_X18_HALL_RECORDER_20260916.md`.
Full diff: `docs/NAVI_X18_RECORDER_VS_X18.diff`.

**It has not been flashed and it has not been bench-tested on hardware.** See
§7 of the report for what must be measured on a bench run before a field run —
the timing, the Hall stack, and above all whether Otto's Wi-Fi carries the
stream all the way round.

---

## Boot lines to verify after flashing

```
[BOOT] NAVI_ONE_1_0X18_LOWLINE_HALL_RECORDER "X18 as flown, plus a continuous 1 kHz Hall record to the Pi. Observation only." — 9950011
[BOOT] X18 LAP-BASELINE FIELD TEST — not field-accepted NAVI_ONE 1.0.
[BOOT] OBSERVATION BUILD: X18 behaviour unchanged; the Hall record has no authority.
[REC] continuous Hall record -> 192.168.68.142:47610  session XXXXXXXX  (observation only; no navigation authority)
```

If it names the other locomotive, the wrong profile was compiled in — see
`LocoConfig.h`, which keeps a history of this repository falling into that
trap.

If the `[REC]` line says **NOT RECORDING**, `XHR_HOST` is not a dotted quad.
It is resolved before Wi-Fi is up, so a hostname there would silently never
record.

---

## Operating it

### 1. Start the receiver on the Pi, BEFORE Otto moves

```bash
ssh david@192.168.68.142
python3 ~/esp-loco-control/tools/xhr_receiver.py --outdir ~/NGR/hall_records
```

### 2. Verify packets are arriving

The receiver prints this within a second of the locomotive booting:

```
  session 3F2A91C4, loco 9950011
  STATUS 1000.0 Hz  ring 1 hi  drops 0/0  udpfail 0  maxgap 1001 us ...
```

`1000.0 Hz` and `drops 0/0` is the stream healthy. From another machine:

```bash
mosquitto_sub -h 192.168.68.142 -t 'ngr/loco/9950011/diag/recorder' -C 1
```

### 3. Stop the capture

`ctrl-c` in the receiver. It flushes, `fsync`s and prints the totals, the
losses and the decode command.

### 4. Decode

```bash
python3 tools/xhr_decode.py ~/NGR/hall_records/xhr_YYYYMMDD_HHMMSS.xhr --report
python3 tools/xhr_decode.py ~/NGR/hall_records/xhr_YYYYMMDD_HHMMSS.xhr \
        --csv samples.csv --rulings rulings.csv
```

**Read the report before the CSV.** If it names missing sequence numbers, that
trace has holes; the report says how wide in milliseconds and whether the
locomotive dropped them or the network did.

### 5. Proving the Pi before the locomotive is involved

```bash
# on the Pi
python3 tools/xhr_receiver.py --outdir /tmp/soak
# anywhere on the railway network
python3 tools/xhr_soak.py --host 192.168.68.142 --seconds 480
```

This proves the receiver, the network path and the card. It proves nothing
about the ADC, the Hall task or the ESP32.

---

## Gates

```bash
sh tests/run_tests.sh
```

All fifteen must pass. Gates 1–13 are X18's own, unchanged. Gate 14 drives the
shipped recorder and writes a real capture; gate 15 reads that capture with the
real decoder and then damages synthetic ones to prove the damage is reported.

---

## Storage

~8.8 kB/s sustained. Two circuits ≈ 4.2 MB, an hour ≈ 32 MB, a 90-minute
session ≈ 48 MB. The receiver stops rather than filling the card below 200 MB
free.
