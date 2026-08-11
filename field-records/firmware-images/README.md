# Preserved firmware images

Rollback images read off a locomotive immediately before a flash. Each is a
full 4 MB dump, and each records the sketch name it contains so the right one
can be identified without flashing it to find out.

| file | loco | sketch | sha256 | read |
|---|---|---|---|---|
| `otto_9950011_pre_QUORUM_1_13.bin` | 9950011 (Otto) | `QUORUM_1_12C` | `934097a5…7633b83` | 2026-08-11 |

Committed gzipped (653 KB vs 4 MB); the raw `.bin` is gitignored. The gzip
restores byte-identically — the sha256 above is of the DECOMPRESSED image, so
verify after unpacking.

Restore with:

```bash
gzip -dc otto_9950011_pre_QUORUM_1_13.bin.gz > /tmp/rollback.bin
shasum -a 256 /tmp/rollback.bin        # must match the table above
esptool --port <PORT> --baud 115200 --chip esp32 write-flash 0x0 /tmp/rollback.bin
```

Note: 460 800 baud failed on Otto's cable with `Invalid head of packet`.
Use 115 200.
