# NAVI_SIMPLIFIED — response to Claude review

**Status:** Reconstructed disposition record. No firmware corrections are included in this document.

| # | Disposition | Required correction and verification | Status |
|---:|---|---|---|
| 1 | Accepted | Put station departure's 200 ms/count value in the upward ramp slot. Host-test departure, missed-stop handback, and phase-timeout handback. | Open |
| 2 | Accepted | Use each profile's `NAVI_BASELINE_ADAPT_PWM`; remove hardcoded 30. Test Otto 24 and Toby 25 boundaries. | Open |
| 3 | Accepted, with architectural clarification | The claimed 500 ms rebound protection does not exist. Do not restore it as a closure-based NAV guard. Implement electrical rearm separately and detection-anchored, interval-specific physical protection. | Open |
| 4 | Accepted | Advance the handled-drop watermark even while position is unknown; act only when appropriate. Test drop-before-declaration. | Open |
| 5 | Accepted | Either wire `StopArmingPolicy` end-to-end or remove the misleading interface after confirming safety semantics. Test safety versus controlled stops. | Open decision |
| 6 | Accepted | Reconcile comments and reversal behavior. Direction changes must be visible evidence, not a silent MM adjustment. | Open |
| 7 | Accepted | Publish truthful sequence length and the actual profile baseline threshold. Test boot payload for both locomotives. | Open |
| 8 | Value accepted; comments rejected | Five-second dwell is the later operator ruling. Update the three stale 30-second comments and preserve the five-second test. | Open |
| 9 | Accepted | Yield or latch safely on event-serial exhaustion; do not busy-loop. Test the terminal state by injection. | Open |
| 10 | Accepted | Honor the documented ADC settle/discard rule in the IR boot probe. | Open |
| 11 | Accepted; central to failed run | Physical protection must remain active during unresolved navigation. Test repeated same-magnet openings throughout ambiguity. | Open |
| 12 | Accepted | Do not choose a new cruise command from a position just discredited without a transparent bounded policy. | Open decision |
| 13 | Accepted | Define safe missed-station behavior; reconcile it with “unsure = skip station stop.” Test every station phase. | Open decision |
| 14 | Accepted | Format the throttle-cap warning from configuration rather than hardcoding 120. | Open |
| 15 | Accepted | Remove dead enum/field or implement their explicit diagnostic purpose. | Open |
| 16 | Accepted | Remove the shadowed timing variable. | Open |
| 17 | Accepted | Replace unsafe/non-atomic volatile increments with a reviewed cross-task accounting mechanism. | Open |
| 18 | Accepted | Put exact sketch/build identity in retained boot identity after every reconnect and in heartbeat fallback; make the dashboard retain/display it. Do not rely on the ordinary alert. | Open |

## Additional correction not fully captured by the review

The failed implementation used a universal 1000 mm/s physical ceiling and gated reachability on known position. The operator later ruled that Hard Protection must use locomotive-, direction-, and mapped-interval-specific recorded timing, anchored at detection, with a permissive fallback when the premise is unreliable. A global PWM-120 envelope is also rejected.

## Review gate

All 18 findings require an explicit code change, explicit no-change rationale, or superseding operator ruling and a corresponding test before another field build is described as flashable.
