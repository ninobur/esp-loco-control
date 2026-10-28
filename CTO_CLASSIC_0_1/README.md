# CTO_CLASSIC_0_1

This folder preserves a four-block CTO/CE ensemble with the MQTT browser console. It is a restore candidate, not a claim that this exact set was field accepted or was the last set flashed. No device or Pi service has been changed.

## Matching components

| Component | File here | Archived source | Radio packet version |
|---|---|---|---|
| Otto or Toby | `locomotive/CTO_CLASSIC_0_1/CTO_CLASSIC_0_1.ino` | `LL_MQ_CO_GC_LO_1_6/LL_MQ_CO_GC_LO_1_6.ino` | 14 |
| Dispatcher | `dispatcher/CTO_CLASSIC_0_1_DISPATCHER/CTO_CLASSIC_0_1_DISPATCHER.ino` | `LL_MQ_CO_GC_DI_1_2/LL_MQ_CO_GC_DI_1_2.ino` | 14 |
| Browser console | `server/cto_classic_0_1.py` | `ngr_app_v1_6.py` | MQTT, not ESP-NOW |

Originals are under `/Users/davidbrown/NGR/_OLD_NGR-Files/`. The sketch filenames, top header names, and serial boot banners were changed to identify this package. The dispatcher copy also omits an old Blynk token that was present only in a comment. The locomotive profile selector was reduced to one active Otto include so it can be switched unambiguously to Toby. No CTO, CE, MQTT topic, or packet behavior was intentionally changed.

The version 17 CE v8.0 sketches are a **different** candidate ensemble. Do not mix them with these version 14 components. The browser app expects MQTT topics from the version 14 control family; copying this app next to v8.0 does not connect the two systems.

## Before compiling or flashing

1. In each sketch folder, copy the `.example.h` profile files to the same names without `.example`. Fill in local WiFi and Blynk credentials. The real credential headers are ignored by Git.
2. For Otto, leave the Otto include active in `LL_LocoConfig.h`. For Toby, comment the Otto include and uncomment the Toby include. Build and flash each locomotive separately; confirm the boot identity before track use.
3. Check the MQTT broker address compiled into both sketches against the Pi address. The browser app connects to Mosquitto on its own host at `127.0.0.1:1883` and serves Flask on port `8080`.
4. Confirm all three ESP32 sketches report packet version 14. A mismatched version is rejected on receive.

The archived dispatcher has a `ceauto10` timer function that is not called from its loop. Manual CE is available; automatic CE should not be expected from this copy. The Work Zone commands are stubs. These source issues have been left unchanged for an explicit review decision.

## Status and rollback

Role: historical restore candidate. Evidence status: dispatcher and both locomotive profiles compiled with ESP32 Arduino core 3.3.12 using local archived credential headers; bench check and track validation are pending. Python syntax was checked. Nothing from this folder has been promoted into `firmware/QUORUM`.

Rollback: the archived source files remain untouched. The repository commit containing this folder identifies the packaged copy; returning to the previous operating build is a separate device-flash and Pi-service action.
