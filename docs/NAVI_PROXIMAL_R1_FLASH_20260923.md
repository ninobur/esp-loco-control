# Toby PROXIMAL_R1 flash

2026-09-23, approximately16:14 PDT. David explicitly authorized flashing.
Source revision b6b7f13, tested artifact at
/private/tmp/navi-proximal-r1-build/NAVI_COHERENCE_0_6_IR_HEALTH.ino.bin.
SHA256:919259c4e00ddd53026f82e90055ee2f9a42990f76151768340efdd1a40bcf77.

Verified USB identity before writing: ESP32-D0WD-V3, MAC ec:e3:34:78:a2:60,
/dev/cu.wchusbserial110. Uploaded at115200 using arduino-cli and ESP32 core3.3.11.
All written hashes verified, successful exit, reset via RTS. No throttle,
AUTO, pairing or position commands sent. No TX/RX/Pi changes.

Post-flash Pi records at16:14:09-17 show:
- health_revision PROXIMAL_R1, shadow0; loco_boot B3950380FC851986.
- PWM0, AUTO0, running0; navigation/direction UNSET.
- Saved IR pairing present, channel11 correct, radio ready.
- No IR packets seen or accepted; health NO_SOURCE, readiness UNAVAILABLE.

Asked David to turn on the IR car and keep it nearby. Do not interpret no
source as an optical fault. Next step is stationary source verification before
manual movement. This is upload/startup verification, not field acceptance or
independent review. Earlier implementation/preparation records saying not
flashed describe their state before this operator-authorized upload.
