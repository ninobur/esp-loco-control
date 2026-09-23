# Away-light repeat: unresolved trial boundaries

Operator clarified previous rotations were closer to 30 seconds and reported
another attempt. Do not score the previous approximately 59-second interval
as ten turns; the later roughly 34-second portion is a candidate, not a
confirmed exact boundary.

Latest capture, same boot 0x4d1e4080e443f121, includes numerous activity periods
after the previous test, with gaps in received reports obscuring segmentation.
The latest clearly separated broad interval is Pi receipt 19:17:19.543 through
19:18:03.942, seq 12497 -> 12939, completed count 4796 -> 5483 (+687).
Another 28 counts appeared 19:18:21.405-19:18:22.367, latest count 5511.
No new saturation or sample-gap counter increases during those periods.

The latest broad interval lasts about 44.4 seconds and cannot be assigned
unconditionally to the operator's approximately 30-second trial. Continued
post-interval activity requires confirmation of whether the wheel was still.
No precise missed/doubled error is assigned to this repeat yet. Prior confirmed
ten-turn results (240 and 274 versus 100) already establish unacceptable
overcount. More unmarked rotations are not needed to establish failure.

Next action: confirm wheel stationary and obtain an explicitly still period
in the current away-light orientation, then analyze the saved waveform.
Raw source archived as `field-records/logs/20260919_ir_away_repeat.log`.
