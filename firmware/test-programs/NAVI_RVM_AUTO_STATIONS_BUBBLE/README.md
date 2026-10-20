# NAVI_RVM_AUTO_STATIONS_BUBBLE

EXPERIMENTAL, not field accepted. It is NAVI_RVM navigation with the 70-count
gate, plus AUTO station stops on the rainbow carpet (PATIO 15, GRILLERS 63,
ARCHES 108, BAMBOO 157) and the two-train bubble (the follower stops 5 MM
early while the leader is in the station). The record is
`docs/NAVI_RVM_AUTO_STATIONS_BUBBLE_20261020.md`.

Build. Otto is the default; Toby needs `-DNAVI_LOCO=9950012`:

    arduino-cli compile --fqbn esp32:esp32:esp32 \
      --build-property "compiler.cpp.extra_flags=-I<repo>/firmware/config -Werror=format -DNAVI_LOCO=9950011" .

Host tests:

    c++ -std=c++17 -O2 -Wall -Wextra -Werror -I. ../../../tests/navi_rvm_bubble/test_bubble.cpp -o /tmp/t && /tmp/t

Operating sequence:

1. Declare both trains (`session_direction` and `start_mm`) in the same direction.
2. Send `cmd/auto 1`, then `cmd/go`.
3. `cmd/stop` pauses AUTO, and `cmd/go` resumes it.
4. `cmd/auto 0` leaves AUTO.
