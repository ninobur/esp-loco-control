#!/usr/bin/env python3
"""USB IR scope. Raw capture is independent of GUI refresh and display pause."""
import argparse
import binascii
import csv
import math
import struct
import sys
import threading
import time
from collections import deque
from datetime import datetime
from pathlib import Path

WIRE = struct.Struct('<IIIQHIIH')
MAGIC = b'BIR1'


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.bad_crc = 0
        self.discarded = 0

    def feed(self, data):
        self.buffer.extend(data)
        result = []
        while len(self.buffer) >= WIRE.size:
            if self.buffer[:4] != MAGIC:
                del self.buffer[0]
                self.discarded += 1
                continue
            packet = self.buffer[:WIRE.size]
            if binascii.crc_hqx(packet[:-2], 0xffff) != int.from_bytes(packet[-2:], 'little'):
                self.bad_crc += 1
                del self.buffer[0]
                continue
            result.append(WIRE.unpack(packet)[1:-1])
            del self.buffer[:WIRE.size]
        return result


class Counter:
    """Fixed hysteresis diagnostic. Requires a low sample before each rise."""
    def __init__(self):
        self.armed = False
        self.count = 0

    def update(self, raw, low, high, discontinuity=False):
        if discontinuity:
            self.armed = False
        rise = False
        if raw <= low:
            self.armed = True
        elif raw >= high and self.armed:
            self.count += 1
            self.armed = False
            rise = True
        return rise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/cu.wchusbserial110')
    parser.add_argument('--outdir', default=str(Path.home() / 'NGR/ir_usb_bench_logs'))
    parser.add_argument('--window', type=float, default=10)
    parser.add_argument('--demo', action='store_true', help='Synthetic signal, never hardware evidence')
    args = parser.parse_args()
    if not 1 <= args.window <= 30:
        parser.error('--window must be 1..30 seconds')
    import serial
    import matplotlib
    matplotlib.use('MacOSX' if sys.platform == 'darwin' else 'TkAgg')
    import matplotlib.pyplot as plt
    from matplotlib.widgets import Button, Slider

    folder = Path(args.outdir)
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / ('ir_usb_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '.csv')
    lock = threading.Lock()
    stop = threading.Event()
    samples = deque(maxlen=int(args.window * 1200))
    edges = deque(maxlen=5000)
    counter = Counter()
    decoder = Decoder()
    state = dict(low=1100, high=1500, paused=False, test=0, boot=None,
                 seq=None, us=None, missed=0, dropped=0, host_missing=0,
                 received=0, sat=0, last_rx=0., error='', rate=0., epoch=0)

    def capture():
        port = None
        try:
            if not args.demo:
                port = serial.Serial()
                port.port, port.baudrate, port.timeout = args.port, 460800, 0.2
                port.dtr = False
                port.rts = False
                port.open()
            with path.open('x', newline='') as output:
                writer = csv.writer(output)
                writer.writerow(('host_time', 'boot', 'seq', 'us', 'raw', 'missed',
                                 'dropped', 'gap', 'low', 'high', 'rise', 'test',
                                 'count', 'bad_crc', 'discarded_bytes', 'demo'))
                start, rate_time, flush_time = time.monotonic(), time.monotonic(), time.monotonic()
                rate_n, demo_seq = 0, 0
                while not stop.is_set():
                    if args.demo:
                        time.sleep(0.02)
                        records = [(1, i, i*1000, int(1400 + 600*math.sin(i*0.02)), 0, 0)
                                   for i in range(demo_seq, demo_seq+20)]
                        demo_seq += 20
                    else:
                        records = decoder.feed(port.read(max(1, min(port.in_waiting, 4096))))
                    rows = []
                    now = time.monotonic()
                    with lock:
                        for boot, seq, us, raw, missed, dropped in records:
                            new_boot = boot != state['boot']
                            delta = 1 if new_boot else (seq-state['seq']) & 0xffffffff
                            lost = 0 if new_boot else max(0, delta-1)
                            gap = new_boot or delta != 1 or missed != state['missed'] or dropped != state['dropped']
                            if not new_boot and us <= state['us']:
                                gap = True
                            if new_boot:
                                samples.clear()
                                edges.clear()
                                counter.count = 0
                                state['epoch'] += 1
                            if gap:
                                samples.append((us/1e6, float('nan')))
                            rise = counter.update(raw, state['low'], state['high'], gap)
                            samples.append((us/1e6, raw))
                            if rise:
                                edges.append((us/1e6, raw))
                            state.update(boot=boot, seq=seq, us=us, missed=missed, dropped=dropped, last_rx=now)
                            state['host_missing'] += lost
                            state['received'] += 1
                            state['sat'] += raw in (0, 4095)
                            rows.append((time.time(), '%08x' % boot, seq, us, raw, missed,
                                         dropped, int(gap), state['low'], state['high'],
                                         int(rise), state['test'], counter.count,
                                         decoder.bad_crc, decoder.discarded, int(args.demo)))
                        rate_n += len(records)
                        if now-rate_time >= 1:
                            state['rate'] = rate_n/(now-rate_time)
                            rate_n, rate_time = 0, now
                    writer.writerows(rows)
                    if now-flush_time >= 0.5:
                        output.flush()
                        flush_time = now
        except Exception as exc:
            with lock:
                state['error'] = str(exc)
        finally:
            if port:
                port.close()

    fig, ax = plt.subplots(figsize=(12, 6))
    fig.subplots_adjust(bottom=0.31, top=0.88)
    ax.set(xlabel='ESP time (seconds)', ylabel='Raw ADC', ylim=(0, 4095), xlim=(0, args.window))
    ax.grid(alpha=0.2)
    ax.set_title('IR USB BENCH' + (' - SYNTHETIC DEMO' if args.demo else ' - GPIO34'))
    trace, = ax.plot([], [], color='#007f9e', lw=0.8)
    marks, = ax.plot([], [], '^', color='#a94100', markersize=5, linestyle='None')
    low_line = ax.axhline(state['low'], color='#008844', lw=1)
    high_line = ax.axhline(state['high'], color='#ba3d00', lw=1)
    status = fig.text(0.08, 0.94, 'Connecting...', fontsize=9)
    fig.text(0.08, 0.015, str(path), fontsize=7)
    low_slider = Slider(fig.add_axes([0.17, 0.19, 0.65, 0.03]), 'Low', 0, 4094, valinit=1100, valstep=1)
    high_slider = Slider(fig.add_axes([0.17, 0.14, 0.65, 0.03]), 'High', 1, 4095, valinit=1500, valstep=1)
    pause_button = Button(fig.add_axes([0.17, 0.055, 0.22, 0.05]), 'Freeze display')
    reset_button = Button(fig.add_axes([0.47, 0.055, 0.22, 0.05]), 'New count')

    def threshold_changed(_):
        low, high = int(low_slider.val), int(high_slider.val)
        if low >= high:
            status.set_text('Low must be below High; previous thresholds remain active')
            return
        with lock:
            state.update(low=low, high=high)
            state['test'] += 1
            counter.armed = False
            counter.count = 0
            edges.clear()
        low_line.set_ydata([low, low])
        high_line.set_ydata([high, high])

    def pause(_):
        with lock:
            state['paused'] = not state['paused']
            paused = state['paused']
        pause_button.label.set_text('Resume display' if paused else 'Freeze display')

    def reset(_):
        with lock:
            state['test'] += 1
            counter.count = 0
            counter.armed = False
            edges.clear()

    low_slider.on_changed(threshold_changed)
    high_slider.on_changed(threshold_changed)
    pause_button.on_clicked(pause)
    reset_button.on_clicked(reset)
    fig.canvas.mpl_connect('key_press_event', lambda e: pause(None) if e.key == ' ' else None)

    def refresh():
        with lock:
            st, count = dict(state), counter.count
            points = [] if st['paused'] else list(samples)
            rises = [] if st['paused'] else list(edges)
        age = time.monotonic()-st['last_rx'] if st['last_rx'] else float('inf')
        health = st['error'] or ('NO DATA' if age > 1 else 'LIVE')
        status.set_text('%s%s | %.0f samples/s | count %d | missed %d dropped %d wire gaps %d CRC %d sat %d' %
                        (health, ' / DISPLAY FROZEN' if st['paused'] else '', st['rate'], count,
                         st['missed'], st['dropped'], st['host_missing'], decoder.bad_crc, st['sat']))
        if points:
            end = points[-1][0]
            points = [p for p in points if p[0] >= end-args.window]
            rises = [p for p in rises if p[0] >= end-args.window]
            trace.set_data(*zip(*points))
            marks.set_data(*zip(*rises) if rises else ([], []))
            ax.set_xlim(max(0, end-args.window), max(args.window, end))
        fig.canvas.draw_idle()

    thread = threading.Thread(target=capture, daemon=True)
    thread.start()
    timer = fig.canvas.new_timer(interval=200)
    timer.add_callback(refresh)
    timer.start()
    plt.show(block=True)
    timer.stop()
    stop.set()
    thread.join(timeout=3)
    print('Saved:', path)


if __name__ == '__main__':
    main()
