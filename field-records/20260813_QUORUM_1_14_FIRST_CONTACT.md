# QUORUM 1.14 — both locomotives flashed; first CTO contact, 2026-08-13

**Both locomotives now run `QUORUM_1_14` from `4b593b6`** (four review rounds,
sixteen findings closed; cleared by CODEX for the supervised two-locomotive
test only).

| | flashed | boot check | port |
|---|---|---|---|
| Toby 9950012 | first | `{"sketch":"QUORUM_1_14","loco":"9950012"}` | usbserial-110 |
| Otto 9950011 | second | `{"sketch":"QUORUM_1_14","loco":"9950011"}` | usbserial-0001 |

`LocoConfig.h` selector was switched per flash and verified by boot line both
times. Hash verified on both uploads.

## First contact — the discovery layer works

With both on 1.14, stationary, undeclared, AUTO off:

```
Toby: rx=146↑ tx=154  expected=9950011  role=NONE  fleet_hold=1
Otto: rx=337↑ tx=573  expected=9950012  role=NONE  fleet_hold=1
```

- **Bidirectional ESP-NOW under live WiFi/MQTT** — both rx counters climbing,
  zero send errors on either side. The radio-coexistence question has its
  first field answer.
- **Membership armed on both**, each naming the other (decision 0034
  first-seen lifecycle).
- **`fleet_hold=1` on both, correctly**: both nav UNSET ⇒ each sees its
  expected peer as not navigating ⇒ decision 0031 stops the fleet. Undeclared
  locomotives holding each other is the strict rule as ruled, observed
  working on first contact. (AUTO off, so no motor implication; the flags and
  alerts published as designed.)
- Roles NONE, properly — derivation requires direction and usable position.

## Anomaly, recorded open: one Toby hang, not reproduced

Toby's FIRST 1.14 boot stopped publishing at uptime ≈180.5 s: retained alert
frozen, no CTO heartbeats, **while the broker's LWT still showed online=1** —
the signature of a hung loop thread with a live network task. Conditions:
solo (Otto still on 1.13, no CTO traffic anywhere), stationary, AUTO off,
~360 ESP-NOW sends in; it coincided with the physical USB cable swap between
locomotives.

After a power cycle, with the receive path fully active (peer traffic at
2 Hz both ways), Toby ran past **7 minutes** with uptime advancing — no
repeat. One occurrence, not reproduced, cause unknown; the cable-handling
window keeps a power/handling explanation live, the loop-hung signature
keeps a firmware explanation live. **Open per the operator's standing
policy.** If it recurs: USB serial on the next boot is the diagnostic;
arduino-esp32 does not watchdog the loop task by default, so a silent hang
needs eyes on the wire.

## Next step (when the operator chooses)

Supervised session step 1: declare both locomotives. Expected observable:
`fleet_hold` drops to 0 on both as each sees the other navigating; then
formation — provisional leader derived, held at its next station.
Deployment and unattended running remain on hold pending the measured
mid-cruise stop gap and the other field gates.

## References

- `docs/QUORUM_1_14_IMPLEMENTATION_REPORT.md` — the four review rounds
- decisions 0031, 0032, 0034; `docs/CTO3/BUBBLE_V1_SPEC.md`
