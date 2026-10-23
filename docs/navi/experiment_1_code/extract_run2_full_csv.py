#!/usr/bin/env python3
"""Read-only extraction of a bounded time window from an NSR1 capture,
written directly as CSV (received_unix_us,sample_t_us,adc,pwm,direction),
matching the format of otto_run2_adc.csv. Binary-searches by recv_us to
locate the window, then streams sequentially through it. Read-only on the
source file.
"""
import sys, struct, csv, gzip, time
sys.path.insert(0, "/home/david/NGR")
import navi_sync_format as F

SRC = sys.argv[1]
LOCO_ID = int(sys.argv[2])
START_US = int(sys.argv[3])
END_US = int(sys.argv[4])
OUT = sys.argv[5]

SCAN_WINDOW = 1 << 21


def read_recv_us_near(fh, offset, filesize):
    offset = max(F.FILE_HDR_LEN, min(offset, filesize - F.FRAME_LEN))
    fh.seek(offset)
    chunk = fh.read(min(SCAN_WINDOW, filesize - offset))
    idx = chunk.find(F.MAGIC)
    if idx < 0 or idx < F.FRAME_LEN:
        return None
    frame_start = offset + idx - F.FRAME_LEN
    fh.seek(frame_start)
    frame = fh.read(F.FRAME_LEN)
    if len(frame) < F.FRAME_LEN:
        return None
    recv_us, length = struct.unpack(F.FRAME_FMT, frame)
    return frame_start, recv_us, length


def main():
    t_wall_start = time.time()
    with open(SRC, "rb") as fh:
        filesize = fh.seek(0, 2)
        fh.seek(0)
        head = fh.read(F.FILE_HDR_LEN)
        magic, started_us, _ = struct.unpack(F.FILE_HDR_FMT, head)
        assert magic == F.FILE_MAGIC, magic

        lo, hi = F.FILE_HDR_LEN, filesize
        for _ in range(40):
            if hi - lo < SCAN_WINDOW:
                break
            mid = (lo + hi) // 2
            got = read_recv_us_near(fh, mid, filesize)
            if got is None:
                hi = mid
                continue
            frame_start, recv_us, _length = got
            if recv_us < START_US:
                lo = frame_start + 1
            else:
                hi = frame_start
        start_offset = max(F.FILE_HDR_LEN, lo - SCAN_WINDOW)

        got = read_recv_us_near(fh, start_offset, filesize)
        if got is None:
            raise SystemExit("could not resync near start_offset %d" % start_offset)
        p = got[0]
        fh.seek(p)

        native_count, bad = 0, 0
        gz = gzip.open(OUT, "wt", newline="")
        w = csv.writer(gz)
        w.writerow(["received_unix_us", "serial", "sample_t_us", "adc", "pwm", "direction"])
        while p < filesize:
            frame = fh.read(F.FRAME_LEN)
            if len(frame) < F.FRAME_LEN:
                break
            recv_us, length = struct.unpack(F.FRAME_FMT, frame)
            data = fh.read(length)
            if len(data) < length:
                break
            p += F.FRAME_LEN + length
            if recv_us > END_US:
                break
            if recv_us < START_US - 2_000_000:
                continue
            try:
                h, payload = F.parse_record(data)
            except F.BadRecord:
                bad += 1
                continue
            if h.loco_id != LOCO_ID or h.rec_type != F.REC_HALL_NATIVE:
                continue
            for item in F.iter_native_hall(h, payload):
                native_count += 1
                w.writerow([recv_us, item["serial"], item["t_us"], item["raw"], item["pwm"], item["direction"]])
        gz.close()
        print("native_count=%d bad_frames=%d start_offset=%d elapsed_s=%.1f" %
              (native_count, bad, start_offset, time.time() - t_wall_start))


if __name__ == "__main__":
    main()
