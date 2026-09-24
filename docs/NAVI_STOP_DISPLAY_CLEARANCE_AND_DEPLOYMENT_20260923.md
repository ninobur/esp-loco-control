# NAVI stop display: clearance and deployment, 2026-09-23

## Independent clearance supplied by the operator

The operator supplied the following review clearance in the task and explicitly
authorized the planned deployment/field check. This is a supplied independent
review, not a claim that Codex performed an independent review of its own work.

> Independent review complete for commit 427c68b.
> OK to proceed.
> I withdraw my earlier objection. I incorrectly treated PWM=0 as though Toby
> might remain in a meaningful coasting state. That does not reflect the
> established behavior of this locomotive/control system.
> The implementation correctly separates raw IR observation from NAVI's
> system-level interpretation. Raw IR may truthfully remain
> INADEQUATE_CONTRAST while NAVI, using commanded PWM=0, actual PWM=0,
> confirmed coupling, fresh/unchanged IR observations, no pulse advance, and
> no Hall advance, concludes STOPPED and reports 0 mm/s to the operator.
> I found no reason in this review to withhold 427c68b. Proceed with the planned
> deployment/field check.

## Dashboard

Exact committed source exported from 427c68b, excluding unrelated uncommitted
Hall baseline/NAV dashboard edits. All three offline dashboard tests and their
rendered-JavaScript assertions passed against that exact candidate.

Installed `/home/david/ngr_app.py` on Pi 192.168.68.142; restarted
`ngr-app.service` and verified active. Live `/loco/toby` responds successfully
and contains the NAVI interpretation fields. Rollback copy:
`/home/david/ngr_app.py.pre_stop_427c68b`.

Source SHA256:
`18c2c898856758ddd3144ce8893f06ed1fa7c939d791a7946e775a743927e2c1`.

## Toby

Verified USB `/dev/cu.wchusbserial110` is ESP32-D0WD-V3 rev 3.1,
MAC `ec:e3:34:78:a2:60`, matching Toby, before writing firmware.
Rebuilt target `esp32:esp32:esp32`: 1,018,459 bytes flash / 73,180 static RAM.
Application SHA256:
`13355c4bcc694e11fe7bb30c16430095fd9823f63415bdd8420ce9e8949a00e8`.

Flashed at 115200 baud approximately 21:56 PDT. All written flash hashes
verified; board reset successfully. Live MQTT confirms
`NAVI_COHERENCE_0_6_PROXIMAL_R1_IR_SPEED_R3`, boot `9CC37A0126AD1E55`.
Fresh IR reports from the expected test car reach telem/ir with the new
navi_speed fields. Initial count 96, raw INADEQUATE_CONTRAST, coupling false;
interpreted speed remains unavailable until genuine coupling confirmation.
This verifies transport and deployment, not the completed physical stop test.

Existing NGR-Files Arduino sketch folder remains a symlink to the canonical
folder, so no new path or duplicate sketch was introduced. This full build
also includes the earlier prepared manual-range R2 change, as disclosed in
the implementation record. IR Test Car and RX firmware are not changed.

## Field check

No throttle, AUTO, direction or coupling commands are sent by Codex during
deployment. Coupling is a physical operator assertion and clears on Toby boot.
The operator must confirm it only when the car is actually coupled.

After coupling confirmation, check stationary STOPPED / 0 pKPH, a short
operator-controlled roll, and return to STOPPED after stopping. Verify positive
moving speed resumes. Raw IR may still report inadequate contrast at rest.
This display deployment is not certification of IR distance accuracy, the
pending TX detector revision or AUTO navigation.
