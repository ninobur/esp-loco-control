#!/usr/bin/env python3
"""
IR_SCOPE_Plotter.py
Ninobur Garden Railway — IR wheel-sensor virtual oscilloscope: live plotter
and CSV logger. Companion to firmware/test-programs/IR_SCOPE/IR_SCOPE.ino.
Interaction model follows NGR HallProbe Plotter 1.1 (automatic CSV logging,
marker support), extended for batched 1 kHz sample streams.

WHAT IT SHOWS
    raw ADC waveform, thrHigh, thrLow, inPulse as a shaded band, rise and
    fall edge markers, text markers, and visible gaps where batches were
    lost. The x-axis is FIRMWARE sample time (sample N = N ms after capture
    start), never MQTT arrival time.

    The merged-pulse condition reads directly off the plot: raw crosses
    thrHigh (rise mark, band starts), the inter-spoke trough stays ABOVE the
    thrLow line, no fall mark appears, and the band continues through the
    next spoke.

DATA HANDLING
    - Batches are reassembled by session id + batch sequence number.
    - A batch-sequence gap or sample discontinuity is logged as a GAP row,
      drawn as a shaded span, and breaks the plotted line (NaN insert).
    - A new session id (firmware reboot) clears the live trace and is logged
      as a SESSION row. Different sessions are never joined.
    - Every reconstructed sample is written to the CSV; markers and gaps are
      self-contained rows. The file is flushed after every batch, so an
      interrupted run stays useful.

KEYS
    space  pause/freeze the display (acquisition + logging continue)

REQUIREMENTS
    pip install paho-mqtt matplotlib

USAGE
    python3 IR_SCOPE_Plotter.py [--broker 192.168.68.142] [--port 1883]
                                [--node irscope01] [--window 10]
                                [--outdir ~/NGR/irscope_logs]

CSV COLUMNS (one row per sample; see README.md for the full contract)
    wall_time, row_type (SAMPLE|MARKER|GAP|SESSION|STATUS), session,
    batch_seq, sample, t_s, raw, run_min, run_max, thr_high, thr_low,
    contrast_valid, in_pulse, rise, fall, info
"""

import argparse
import csv
import json
import math
import os
import threading
import time
from collections import deque
from datetime import datetime

import matplotlib
matplotlib.use("TkAgg")          # change to "MacOSX" if TkAgg is missing
import matplotlib.pyplot as plt
import paho.mqtt.client as mqtt

# ============================================================================
# CONFIGURATION / ARGUMENTS
# ============================================================================
parser = argparse.ArgumentParser(description="NGR IR_SCOPE live plotter/logger")
parser.add_argument("--broker", default="192.168.68.142")
parser.add_argument("--port",   type=int, default=1883)
parser.add_argument("--node",   default="irscope01")
parser.add_argument("--window", type=float, default=10.0,
                    help="Seconds of data visible in the scrolling window")
parser.add_argument("--outdir", default=os.path.expanduser("~/NGR/irscope_logs"),
                    help="Directory for CSV log files")
args = parser.parse_args()

TOPIC_SAMPLES = "ngr/diag/%s/samples" % args.node
TOPIC_MARKER  = "ngr/diag/%s/marker"  % args.node
TOPIC_STATUS  = "ngr/diag/%s/status"  % args.node

SAMPLE_HZ  = 1000
MAX_POINTS = int(args.window * SAMPLE_HZ * 4)   # generous buffer beyond window

FLAG_INPULSE  = 0x1000
FLAG_RISE     = 0x2000
FLAG_FALL     = 0x4000
FLAG_CONTRAST = 0x8000

# ============================================================================
# CSV LOG
# ============================================================================
os.makedirs(args.outdir, exist_ok=True)
session_ts = datetime.now().strftime("%Y%m%d_%H%M%S")
log_path   = os.path.join(args.outdir, "irscope_%s.csv" % session_ts)
log_file   = open(log_path, "w", newline="")
csv_writer = csv.writer(log_file)
csv_writer.writerow([
    "wall_time", "row_type", "session", "batch_seq", "sample", "t_s",
    "raw", "run_min", "run_max", "thr_high", "thr_low",
    "contrast_valid", "in_pulse", "rise", "fall", "info"
])
log_file.flush()


def wall_now():
    return datetime.now().isoformat(timespec="milliseconds")


# ============================================================================
# SHARED DATA (MQTT thread -> plot loop)
# ============================================================================
lock = threading.Lock()

t_data    = deque(maxlen=MAX_POINTS)   # firmware seconds (sample/1000)
raw_data  = deque(maxlen=MAX_POINTS)
hi_data   = deque(maxlen=MAX_POINTS)
lo_data   = deque(maxlen=MAX_POINTS)
ip_data   = deque(maxlen=MAX_POINTS)   # inPulse 0/1
cv_data   = deque(maxlen=MAX_POINTS)   # contrast-valid 0/1

rise_pts = deque(maxlen=4000)          # (t, raw)
fall_pts = deque(maxlen=4000)
markers  = deque(maxlen=200)           # (t, text)
gaps     = deque(maxlen=200)           # (t_start, t_end_or_None)

state = {
    "sid": None,
    "next_seq": None,
    "last_sample_end": None,           # first+n of the previous batch
    "missing_batches": 0,
    "batches_rx": 0,
    "samples_rx": 0,
    "fw_late_us": 0,                   # worst batch lateness seen
    "fw_late_n": 0,
    "fw_bdrop": 0,
    "fw_latch": 0,
    "fw_closs": 0,
    "fw_sat": 0,
    "fw_pulses": 0,
    "last_seq": None,
    "last_sample": None,
    "in_pulse": 0,
    "contrast_valid": 0,
    "status_note": "waiting for ESP32...",
    "rx_window": deque(maxlen=50),     # (wall_ts, n_samples) for arrival rate
}


def log_row(row_type, session, batch_seq, sample, t_s, raw="", mn="", mx="",
            hi="", lo="", cv="", ip="", rise="", fall="", info=""):
    csv_writer.writerow([
        wall_now(), row_type, session, batch_seq, sample,
        ("%.3f" % t_s) if t_s != "" else "",
        raw, mn, mx, hi, lo, cv, ip, rise, fall, info
    ])


# ============================================================================
# BATCH DECODING
# ============================================================================
def handle_batch(d):
    sid   = d.get("sid")
    seq   = d.get("seq")
    first = d.get("first")
    n     = d.get("n")
    hexs  = d.get("hex", "")
    env   = d.get("env")
    if sid is None or seq is None or first is None or n is None or env is None:
        return
    if len(hexs) != n * 4:
        return

    with lock:
        # --- session boundary: never join traces across a reboot -----------
        if sid != state["sid"]:
            if state["sid"] is not None:
                log_row("SESSION", sid, seq, first, first / 1000.0,
                        info="new session %s (was %s) — trace cleared" % (sid, state["sid"]))
                t_data.clear(); raw_data.clear(); hi_data.clear()
                lo_data.clear(); ip_data.clear(); cv_data.clear()
                rise_pts.clear(); fall_pts.clear()
                markers.clear(); gaps.clear()
            else:
                log_row("SESSION", sid, seq, first, first / 1000.0,
                        info="session %s" % sid)
            state["sid"] = sid
            state["next_seq"] = seq
            state["last_sample_end"] = first

        # --- discontinuity detection ---------------------------------------
        missing = 0
        if state["next_seq"] is not None and seq != state["next_seq"]:
            missing = seq - state["next_seq"]
        sample_jump = (state["last_sample_end"] is not None
                       and first != state["last_sample_end"])
        if (missing > 0 or sample_jump) and t_data:
            gap_from = state["last_sample_end"] / 1000.0
            gap_to   = first / 1000.0
            state["missing_batches"] += max(missing, 0)
            info = ("missing %d batch(es), samples %d..%d (%.3f s)"
                    % (max(missing, 0), state["last_sample_end"], first - 1,
                       gap_to - gap_from))
            log_row("GAP", sid, seq, first, gap_from, info=info)
            gaps.append((gap_from, gap_to))
            # break the plotted line so the hole is visible, never bridged
            t_data.append(float("nan")); raw_data.append(float("nan"))
            hi_data.append(float("nan")); lo_data.append(float("nan"))
            ip_data.append(0); cv_data.append(0)
        state["next_seq"] = seq + 1
        state["last_sample_end"] = first + n

        # --- envelope schedule: start values + mid-batch updates ------------
        env_sched = [(0, env[0], env[1], env[2], env[3])]
        for u in d.get("envu", []):
            if len(u) == 5:
                env_sched.append((u[0], u[1], u[2], u[3], u[4]))
        env_sched.sort(key=lambda e: e[0])

        ei = 0
        mn, mx, hi, lo = env_sched[0][1:]
        for i in range(n):
            while ei + 1 < len(env_sched) and env_sched[ei + 1][0] <= i:
                ei += 1
                mn, mx, hi, lo = env_sched[ei][1:]
            v = int(hexs[i * 4:i * 4 + 4], 16)
            raw  = v & 0x0FFF
            ip   = 1 if (v & FLAG_INPULSE)  else 0
            rise = 1 if (v & FLAG_RISE)     else 0
            fall = 1 if (v & FLAG_FALL)     else 0
            cv   = 1 if (v & FLAG_CONTRAST) else 0
            sample = first + i
            t = sample / 1000.0

            t_data.append(t); raw_data.append(raw)
            hi_data.append(hi); lo_data.append(lo)
            ip_data.append(ip); cv_data.append(cv)
            if rise:
                rise_pts.append((t, raw))
            if fall:
                fall_pts.append((t, raw))
            log_row("SAMPLE", sid, seq, sample, t, raw, mn, mx, hi, lo,
                    cv, ip, rise, fall)

        state["batches_rx"] += 1
        state["samples_rx"] += n
        state["fw_late_us"] = max(state["fw_late_us"], d.get("late_us", 0))
        state["fw_late_n"]  = d.get("late_n", state["fw_late_n"])
        state["fw_bdrop"]   = d.get("bdrop", state["fw_bdrop"])
        state["fw_latch"]   = d.get("latch", state["fw_latch"])
        state["fw_closs"]   = d.get("closs", state["fw_closs"])
        state["fw_sat"]     = d.get("sat", state["fw_sat"])
        state["fw_pulses"]  = d.get("pulses", state["fw_pulses"])
        state["last_seq"]   = seq
        state["last_sample"] = first + n - 1
        state["in_pulse"] = ip_data[-1]
        state["contrast_valid"] = cv_data[-1]
        state["rx_window"].append((time.time(), n))

    log_file.flush()


def handle_marker(payload):
    text = payload.strip()
    sample = None
    try:
        d = json.loads(payload)
        text = d.get("text", text)
        sample = d.get("sample")
    except (json.JSONDecodeError, AttributeError):
        pass
    with lock:
        if sample is not None:
            t = sample / 1000.0
        else:
            t = t_data[-1] if t_data else 0.0
        markers.append((t, text))
        log_row("MARKER", state["sid"] or "", "", sample or "", t, info=text)
    log_file.flush()
    print("[Plotter] Marker @ t=%.3fs : %s" % (t, text))


def handle_status(payload):
    try:
        d = json.loads(payload)
    except json.JSONDecodeError:
        with lock:
            state["status_note"] = payload.strip()[:80]
        return
    with lock:
        state["status_note"] = ("fw %s sid %s heap %s rssi %s"
                                % (d.get("fw", "?"), d.get("sid", "?"),
                                   d.get("heap", "?"), d.get("rssi", "?")))
        log_row("STATUS", d.get("sid", ""), "", d.get("samples", ""), "",
                info=payload.strip()[:300])
    print("[Plotter] status: %s" % payload.strip())


# ============================================================================
# MQTT
# ============================================================================
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("[Plotter] Connected to %s:%d" % (args.broker, args.port))
        client.subscribe(TOPIC_SAMPLES)
        client.subscribe(TOPIC_MARKER)
        client.subscribe(TOPIC_STATUS)
    else:
        print("[Plotter] MQTT connect failed rc=%s" % rc)


def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8", errors="ignore")
    if msg.topic == TOPIC_SAMPLES:
        try:
            d = json.loads(payload)
        except json.JSONDecodeError:
            return
        handle_batch(d)
    elif msg.topic == TOPIC_MARKER:
        handle_marker(payload)
    elif msg.topic == TOPIC_STATUS:
        handle_status(payload)


def mqtt_thread_fn():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id="ngr-irscope-plotter")
    client.on_connect = on_connect
    client.on_message = on_message
    while True:
        try:
            client.connect(args.broker, args.port, keepalive=15)
            client.loop_forever()
        except Exception as e:
            print("[Plotter] MQTT error: %s — retrying in 5s" % e)
            time.sleep(5)


# ============================================================================
# PLOT
# ============================================================================
paused = {"on": False}


def on_key(event):
    if event.key == " ":
        paused["on"] = not paused["on"]
        print("[Plotter] display %s (acquisition and logging continue)"
              % ("FROZEN" if paused["on"] else "live"))


def main():
    print("[Plotter] NGR IR_SCOPE plotter")
    print("[Plotter] Broker : %s:%d  node %s" % (args.broker, args.port, args.node))
    print("[Plotter] Window : %.1fs   (space = pause display)" % args.window)
    print("[Plotter] Log    : %s" % log_path)

    threading.Thread(target=mqtt_thread_fn, daemon=True).start()

    fig, ax = plt.subplots(figsize=(13, 6))
    fig.canvas.mpl_connect("key_press_event", on_key)
    fig.patch.set_facecolor("#1a1a2e")
    ax.set_facecolor("#0f0f23")
    ax.set_title("NGR IR_SCOPE — raw waveform vs live detector thresholds",
                 color="#e0e0e0", fontsize=13, pad=10)
    ax.set_xlabel("Firmware time (s, sample/1000)", color="#a0a0b0")
    ax.set_ylabel("ADC counts", color="#a0a0b0")
    ax.tick_params(colors="#a0a0b0")
    for spine in ax.spines.values():
        spine.set_edgecolor("#333355")

    line_raw, = ax.plot([], [], color="#00d4ff", lw=1.0, label="raw", zorder=4)
    line_hi,  = ax.plot([], [], color="#ff6b35", lw=1.0, label="thrHigh", zorder=3)
    line_lo,  = ax.plot([], [], color="#7bed9f", lw=1.0, label="thrLow", zorder=3)
    ax.legend(facecolor="#1a1a2e", edgecolor="#333355", labelcolor="#e0e0e0",
              fontsize=9, loc="upper right")

    fig.text(0.01, 0.01, "Logging -> %s" % log_path, color="#505070",
             fontsize=7, fontfamily="monospace")
    status_obj = fig.text(0.01, 0.035, "Waiting...", color="#9090b0",
                          fontsize=8, fontfamily="monospace")

    transient = []          # artists rebuilt each frame
    drawn_marks = {}

    plt.tight_layout(rect=[0, 0.06, 1, 1])
    plt.ion()
    plt.show()

    while plt.fignum_exists(fig.number):
        if paused["on"]:
            plt.pause(0.1)
            continue
        with lock:
            if not t_data:
                plt.pause(0.1)
                continue
            t_arr  = list(t_data)
            r_arr  = list(raw_data)
            hi_arr = list(hi_data)
            lo_arr = list(lo_data)
            ip_arr = list(ip_data)
            rp     = list(rise_pts)
            fp     = list(fall_pts)
            mk     = list(markers)
            gp     = list(gaps)
            st     = dict(state)
            rxw    = list(state["rx_window"])

        t_now = next((t for t in reversed(t_arr) if not math.isnan(t)), 0.0)
        t_min = t_now - args.window
        i0 = next((i for i, t in enumerate(t_arr)
                   if not math.isnan(t) and t >= t_min), 0)

        tw = t_arr[i0:]
        line_raw.set_data(tw, r_arr[i0:])
        line_hi.set_data(tw, hi_arr[i0:])
        line_lo.set_data(tw, lo_arr[i0:])

        for a in transient:
            a.remove()
        transient = []

        # inPulse shaded band
        band = ax.fill_between(tw, 0, 4095,
                               where=[v > 0 for v in ip_arr[i0:]],
                               color="#3355ff", alpha=0.18, zorder=1)
        transient.append(band)

        # gap shading
        for (g0, g1) in gp:
            if g1 >= t_min:
                transient.append(ax.axvspan(max(g0, t_min), min(g1, t_now + 1),
                                            color="#ff2244", alpha=0.25, zorder=2))

        # edges
        rr = [(t, v) for (t, v) in rp if t >= t_min]
        ff = [(t, v) for (t, v) in fp if t >= t_min]
        if rr:
            transient.append(ax.scatter([p[0] for p in rr], [p[1] for p in rr],
                                        marker="^", s=45, color="#ffd700",
                                        zorder=5, label="rise"))
        if ff:
            transient.append(ax.scatter([p[0] for p in ff], [p[1] for p in ff],
                                        marker="v", s=45, color="#ff4d6d",
                                        zorder=5, label="fall"))

        # markers
        for (tm, text) in mk:
            if tm >= t_min:
                transient.append(ax.axvline(tm, color="#ffd700", lw=1.0,
                                            linestyle=":", alpha=0.85, zorder=4))
                short = text[:25] + ("..." if len(text) > 25 else "")
                transient.append(ax.text(tm + 0.02, 4000, short,
                                         color="#ffd700", fontsize=7,
                                         rotation=90, va="top", alpha=0.9))

        ax.set_xlim(t_min, t_now + 0.05 * args.window)
        vis = [v for v in r_arr[i0:] if not math.isnan(v)]
        if vis:
            lo_v = min(vis + [v for v in lo_arr[i0:] if not math.isnan(v)])
            hi_v = max(vis + [v for v in hi_arr[i0:] if not math.isnan(v)])
            pad = max(50, (hi_v - lo_v) * 0.15)
            ax.set_ylim(max(0, lo_v - pad), min(4300, hi_v + pad))

        # arrival-rate estimate over the rx window
        rate = 0.0
        if len(rxw) >= 2:
            dt = rxw[-1][0] - rxw[0][0]
            if dt > 0:
                rate = sum(n for (_, n) in rxw[1:]) / dt

        status_obj.set_text(
            "sid %s  batch %s  sample %s | rx %.0f sa/s | fw late max %d us n=%d"
            " | fw bdrop %d  plotter-missing %d | latch %d closs %d sat %d"
            " | pulses %d | inPulse=%d contrast=%d %s"
            % (st["sid"], st["last_seq"], st["last_sample"], rate,
               st["fw_late_us"], st["fw_late_n"], st["fw_bdrop"],
               st["missing_batches"], st["fw_latch"], st["fw_closs"],
               st["fw_sat"], st["fw_pulses"], st["in_pulse"],
               st["contrast_valid"], st["status_note"]))

        fig.canvas.draw_idle()
        plt.pause(0.15)

    log_file.close()
    print("\n[Plotter] Session log saved -> %s" % log_path)


if __name__ == "__main__":
    main()
