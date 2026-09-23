# Repeater presence check, September 20

After the operator enabled the repeater, a read-only MQTT subscription to
`ngr/survey/#` observed fresh ESPNOW_REP_2 health messages at uptime 105009
and 110009 ms. Mode repeat, channel 11, sources 0, foreign frames 777 then
812, dropped 0, txDone 0, txFailed 0, AP RSSI -68 then -65 dBm.
The retained site label MAC_BENCH is not verified physical placement.
REP_1's retained online value was 0.

REP_2 is demonstrably online, but no forwarding was reported. The repository
ESPNOW_REPEATER sketch accepts CTO status magic 0xC4/version 3 and role echo
0xC5/version 1 with exact struct lengths. It has no IR type-1/type-5 forwarding
path. Foreign traffic is not decoded in health messages, so the increasing
foreign counter alone cannot identify its sender.

The Pi IR RX continued receiving the car's e4410597 session. Its serial output
omits source MAC addresses, so that stream alone cannot identify a relay hop.
Do not attribute any reception improvement to the repeater. IR support would
require an explicit firmware change; none was made during this check.
