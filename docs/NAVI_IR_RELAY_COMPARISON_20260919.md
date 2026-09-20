# IR relay delivery comparison, September 19

Read-only capture inspection at approximately 20:13 PDT. Operator reported
relay power-up near the 20:10:05 screenshot, but exact switch time is unknown.
This is a provisional observational comparison, not a controlled relay test.

Source: Pi /home/david/NGR/ir_espnow/ir_espnow_raw_20260919.log;
download snapshot /private/tmp/ir_relay_comparison_20260919.log.
Movement boot 0x2ce42ee145582bbf; raw session 0x45582bbf, unchanged.

Compare three full minute bins 20:07-20:10 with 20:10-20:13:

| Stream | Before missing sequence IDs | After missing sequence IDs |
| --- | --- | --- |
| Raw waveform batches | 1049 / 1840 (57.0%) | 917 / 1849 (49.6%) |
| Movement snapshots | 864 / 1730 (49.9%) | 800 / 1788 (44.7%) |

Each denominator is the sum of first-to-last sequence spans within each minute;
missing reports crossing minute edges are not counted. Unique sequences avoid
duplicate inflation. These are missing generated reports at the recorder,
not proven over-air loss: snapshot queue overwrites or other transport loss
can also contribute. All inspected relevant packets from 20:00 onward passed
payload and recorder CRC checks. No receiver-side timeout policy was assumed.

Movement report longest within-minute receipt gaps before: 1.186, 3.480,
2.272 seconds. After: 2.599, 2.248, 3.457 seconds. Thus fewer missing IDs
did not eliminate multi-second stale telemetry periods. These are receipt
gaps, not the sensor's SIGNAL_STALE condition.

Every received movement snapshot between 20:00 and the partial 20:13 minute
reported TRACKING. Within each minute cumulative sample_gaps, saturated_samples,
open_aborts and unreliable_samples had zero increments. Raw missed-sample,
queue-drop and immediate send-error counters also had zero increments within
those bins. Completed counts continued growing. No claim of pulse-count
accuracy follows from this; distance_validated remains zero in this protocol.

Interpretation: modest delivery improvement in the chosen windows, but no
demonstrated causal relay benefit. Earlier minutes already ranged widely
(movement missing 29.6% at 20:07 and 80.9% at 20:05). Different track positions
and an approximate switch time confound this short comparison. No relay
forwarding compatibility or packet route was established by this inspection.

Operational consequence: wiring IR to the locomotive ESP removes this radio
hop from the navigation measurement path. It does not change the need to
validate missed/doubled pulses and illumination behavior locally. The operator's
no-strike report is separate evidence and was not re-audited here.
