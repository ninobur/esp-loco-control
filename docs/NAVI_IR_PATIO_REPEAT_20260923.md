# Patio repeat after CAL0_FIX1

David clarified that the preceding trial included hand rolling, a pause, then
a 5-10 second patio roll. Do not label its contrast transitions as confirmed
faults during continuous motion. He then repeated a requested continuous
approximately ten-second roll, reporting start and done.

Repeat start was noted at approximately 14:02:16 PDT. Pi serial raw type-5
records show 458 pulses before the roll and 683 afterward: 225 counted pulses,
nominal 2171.7 mm at 9.652 mm/pulse. No independently measured travel or turn
count was supplied, so this is not an accuracy or missed-pulse measurement.

First received TRACKING report: 14:02:16.060645, seq 24069, 464 pulses.
Last received TRACKING before the reception gap: 14:02:20.850241, seq 24120,
642 pulses. Next received report: 14:02:26.610455, seq 24174, 683 pulses,
INADEQUATE_CONTRAST. TX capture-time gap between those snapshots: 5.408 seconds.
The 26 received TRACKING snapshots in the selected roll window contained no
reported optical interruption. Coverage is incomplete; continuity throughout
the roll cannot be certified. Sample-gap counter stayed at 1, saturation and
open-abort counters at 0 in the examined records. Missing transport snapshots
are not evidence of missed wheel pulses, and unchanged counters cannot prove
the absence of transient contrast faults in an unobserved interval.

At the initial post-test check (Pi 14:02:43), latest logged Toby health was
still timestamped 14:02:15.499. This prevents treating MQTT health as live
through the repeat. Raw RX data independently confirms pulse accumulation and
TRACKING recovery. Cause of telemetry delay/missing snapshots not established.

Conclusion: movement recovery observed; stationary contrast limitation remains;
distance accuracy and complete on-loco epoch continuity remain unverified.
No firmware, Pi, or motor changes were made for this test.
