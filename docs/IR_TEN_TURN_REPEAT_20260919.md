# Ten-turn repeat: confirmed overcount

Operator clarified the first run: turned car over, then ten constant full
rotations; wheel has ten spokes. Therefore the later first-run interval's
240 counts correspond to 100 expected pulses, net excess 140 (140%). The
earlier 106-count handling activity is excluded.

Repeat instructions: leave car inverted, stationary ten seconds, ten full
rotations in about thirty seconds, stationary ten seconds, same light/USB.
Operator reported "done".

Same boot 0x4d1e4080e443f121. Repeat activity:
- Start seq 8567, Pi receipt 19:10:46.563 PDT, completed count 346.
- End seq 8919, Pi receipt 19:11:21.788 PDT, completed count 620.
- Delta 274 for 100 expected: net excess 174 (174%).
- Activity duration about 35.2 seconds.
- No increase in sample-gap or saturation counters over this interval.
- By seq 9117, roughly 19.8 seconds later, count remained 620.
- Inferred corrections remain zero; distance remains explicitly unvalidated.

Both ten-turn tests fail. Excess is not a constant calibration factor (2.40x
then 2.74x); changing wheel circumference or dividing counts is unjustified.
Endpoint totals show net excess, not separate exact missed/doubled counts.
Raw waveform analysis is required to characterize the additional crossings.
Do not enable NAVI distance bounds based on these counts.

Raw evidence: `field-records/logs/20260919_ir_ten_turn_repeat.log`.

Raw inspection of this window: 347 received batches, 248 fully observed
rise/fall pairs after excluding pairs across packet gaps. Median high width
49.5 ms; widths <=2/5/10/20 ms numbered 20/26/37/63. Reported envelope span
min/median/max 431/495/559. Thus not all excess activity is brief chatter;
a short glitch filter alone is not yet a supported solution. Received raw
pairs are incomplete because radio gaps omit samples and boundary crossings.
