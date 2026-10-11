# NAVI_EWO_0_3

Complete, isolated NAVI restoration review candidate. Open
[NAVI_EWO_0_3.ino](NAVI_EWO_0_3.ino). Foundation: operational 0.1 commit `85a2cf2`.
Selected profile: Otto `9950011`; the hardware profile and credentials inclusion
method are inherited unchanged. Not flashed, merged, deployed, or field accepted.

[Implementation status and limits](STATUS.md) · [Verification results](VERIFICATION.md)

Run the complete offline suite:

```sh
sh firmware/programs/NAVI_EWO_0_3/run_tests.sh
```

Build using ESP32 Arduino core 3.3.12, board `esp32:esp32:esp32`, PubSubClient 2.8,
Adafruit INA219 1.2.3, and Adafruit BusIO 1.17.4. Supply the existing untracked
`credentials.h` through the normal external include directory. Validation used
only the repository's safe placeholder template; no credential contents were
read, committed, or changed. No upload command is part of this workflow.
