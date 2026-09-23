# Ninobur Garden Railway — locomotive control

G-scale outdoor railway, 171 magnetic markers, ESP32 locomotives coordinating
over MQTT and ESP-NOW.

## Layout

```
firmware/programs/               all runnable development and experimental sketches
firmware/programs/QUORUM/        current QUORUM-lineage development sketch
firmware/reference/              non-runnable architecture and package material
firmware/README.md               authoritative sketch roles and validation-status catalog
firmware/config/        shared headers, symlinked into each sketch folder
server/                 Raspberry Pi — Flask dashboard, dispatcher, loggers
tools/                  align_markers.py and other analysis run by hand
docs/                   plans, specs and review notes
archive/                superseded versions, kept for the record
```

## Building

Point the Arduino IDE at this repository once:

**Settings → Sketchbook location → `<repo>/firmware`**

`QUORUM` — and the sketches under `programs` — then appear under
**File → Sketchbook** and compile in place. There is no second copy anywhere, so what you edit is what you flash.

**Upload Speed must be 115200.** The default 921600 fails on this adapter, and
the setting reverts whenever the board selection changes.

`credentials.h` is not in the repository. Copy `firmware/config/credentials_template.h`
into each sketch folder as `credentials.h` and fill it in.

## Versions

The filename does not carry the version. `QUORUM.ino` is always current;
versions are git tags. (Tags up to v2.22 predate the rename and hold the
sketch at `firmware/SOLONAV/SOLONAV.ino` — use that path when diffing them.)

“Current source” and “field-tested evidence” are deliberately different
concepts. There is currently no designated production program. See
`firmware/README.md` before selecting a sketch to flash or describing a
capability as ready for operation.

```bash
git tag -a v2.17 -m "what changed and why"
git diff v2.16 v2.17 -- firmware/SOLONAV/SOLONAV.ino
git checkout v2.16 -- firmware/SOLONAV/SOLONAV.ino
```

Tag only what gets flashed. Untagged commits are drafts.

The running firmware publishes its identity to `ngr/loco/<id>/state/bootid`, so
what is on the locomotive can always be checked against what is in the repo.

## Where to start

`docs/ROAD_TO_CTO.md` — the milestone plan and its current state.
