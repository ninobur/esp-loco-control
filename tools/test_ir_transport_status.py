#!/usr/bin/env python3
"""Actual transmitter status construction -> Pi parser/logger regression."""
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import types

from ir_pulse_transport import (PULSE_WIRE, STATUS_FIELDS, STATUS_WIRE,
                                TransportTelemetry, crc16, decode, record_line)
from ir_movement_decode import WIRE as MOVEMENT_WIRE, decode as decode_movement

ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / 'firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX'
SOURCE = (SKETCH / 'IR_SCOPE_ESPNOW_TX.ino').read_text()


def function(source, name):
    start = source.rfind('static ', 0, source.index(name))
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def seal(data):
    return data[:-2] + struct.pack('<H', crc16(data[:-2]))


def rx(data):
    return ('RX 1234 -70 %d %04x %s\n' % (len(data), crc16(data), data.hex())).encode()


def pulse(sequence, boot=123, sid=42):
    return seal(PULSE_WIRE.pack(0x4952, 1, 6, sid, sequence, boot,
                               sequence or 2**32, 2**33 + sequence * 40000,
                               40000, (sequence or 2**32) * 9652, 9652, 6, 1000, 0))


def status(sequence=1, boot=123, sid=42):
    return seal(STATUS_WIRE.pack(0x4952, 1, 7, sid, sequence, boot,
                                *range(101, 113), 0))


class StatusTests(unittest.TestCase):
    def test_actual_recorder_loop_and_health(self):
        import ir_scope_serial_record as recorder

        class Finished(Exception):
            pass

        class Receiver:
            def __enter__(self):
                self.lines = iter([rx(pulse(4)), rx(status()), rx(pulse(6)),
                                   rx(status(2)), b'STAT rx=4 qdrop=0 badlen=0 q=0\n', b''])
                return self

            def __exit__(self, *args):
                pass

            def readline(self):
                try:
                    return next(self.lines)
                except StopIteration:
                    raise Finished()

        with tempfile.TemporaryDirectory() as tmp:
            args = ['recorder', '--outdir', tmp]
            with patch('sys.argv', args), patch.dict('sys.modules', {
                    'serial': types.SimpleNamespace(Serial=lambda *a, **k: Receiver())}), \
                    patch.object(recorder.time, 'time', side_effect=range(100, 200, 6)):
                with self.assertRaises(Finished):
                    recorder.main()
            raw = next(Path(tmp).glob('ir_espnow_raw_*.log')).read_bytes()
            self.assertEqual(raw.count(b'PULSE_TRANSPORT '), 2)
            self.assertIn(rx(pulse(4)), raw)
            health = json.loads((Path(tmp) / 'ir_espnow_health.json').read_text())
            stream = health['pulse_transport']['streams']['0000002a:000000000000007b']
            self.assertEqual(stream['type6']['received'], 2)
            self.assertEqual(stream['type6']['forward_gaps'], 1)
            self.assertEqual(stream['type7']['received'], 2)
            self.assertEqual(health['pulse_transport']['receiver_status']['qdrop'], 0)
            self.assertEqual(health['lines'], 5)
            self.assertEqual(health['bytes'], len(raw))

    def test_actual_firmware_builder_and_cadence(self):
        harness = r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "PulseTransportStatus.h"
static uint32_t clockMs=0,pulseStatusAt=0,pulseStatusSequence=0,calls=0;
static uint16_t MAGIC=0x4952;static uint8_t VERSION=1;
static uint32_t sid=42;static uint64_t movementBoot=123;
static std::atomic<uint32_t> pulseEventGenerated{101},pulseEventSent{102},
  pulseEventFailed{103},pulseEventDrops{104},pulseLogDrops{105},pulseQueueHigh{107},
  pulseSendLagMaxUs{109},radioTimeouts{110},radioBusyDrops{111},sendErrors{112};
static unsigned pulseEventQueue=106,pulseLogQueue=108;
static uint32_t millis(){return clockMs;}
static unsigned uxQueueMessagesWaiting(unsigned q){return q;}
static PulseTransportStatusPacket last;
static bool radioSend(const uint8_t* data,size_t size){
  assert(size==70);std::memcpy(&last,data,size);++calls;return false;
}
'''
        harness += function(SOURCE, 'crc16(') + '\n'
        harness += function(SOURCE, 'servicePulseTransportStatus(')
        harness += r'''
int main(){
  assert(sizeof(PulseTransportStatusPacket)==70);
  assert(offsetof(PulseTransportStatusPacket,sid)==4);
  assert(offsetof(PulseTransportStatusPacket,sequence)==8);
  assert(offsetof(PulseTransportStatusPacket,sendErrors)==64);
  servicePulseTransportStatus();assert(calls==0);
  clockMs=999;servicePulseTransportStatus();assert(calls==0);
  clockMs=1000;servicePulseTransportStatus();assert(calls==1 && last.sequence==1);
  for(const auto b: *reinterpret_cast<const uint8_t (*)[70]>(&last))std::printf("%02x",b);
  std::puts("");
  clockMs=1999;servicePulseTransportStatus();assert(calls==1);
  clockMs=8000;servicePulseTransportStatus();assert(calls==2 && last.sequence==2);
  servicePulseTransportStatus();assert(calls==2); // No burst after a stalled radio.
  assert(last.generated==101 && last.sent==102 && last.failed==103 && last.dropped==104);
  pulseStatusAt=UINT32_MAX-499;clockMs=500;pulseStatusSequence=UINT32_MAX;
  servicePulseTransportStatus();assert(calls==3 && last.sequence==0);
  pulseEventGenerated=UINT32_MAX;pulseEventSent=UINT32_MAX;
  clockMs+=1000;servicePulseTransportStatus();assert(last.generated==UINT32_MAX);
  assert(last.crc==crc16((const uint8_t*)&last,68));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp) / 'status.cpp', Path(tmp) / 'status'
            cpp.write_text(harness)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-I', str(SKETCH),
                            str(cpp), '-o', str(exe)], check=True)
            data = bytes.fromhex(subprocess.check_output([str(exe)], text=True).strip())
        self.assertEqual(data, status())
        telemetry = TransportTelemetry()
        stream = io.BytesIO()
        size = record_line(stream, rx(data), 123.456, telemetry)
        self.assertEqual(size, len(stream.getvalue()))
        lines = stream.getvalue().splitlines()
        self.assertEqual(lines[0], b'123.456000 ' + rx(data).rstrip(b'\n'))
        row = json.loads(lines[1].split(b' PULSE_TRANSPORT ')[1])
        self.assertEqual(row['received_epoch'], 123.456)
        self.assertEqual(row['boot'], '000000000000007b')
        for field, value in zip(STATUS_FIELDS[6:-1], range(101, 113)):
            self.assertEqual(row[field], value)

    def test_crc_and_schema_rejection(self):
        good = status()
        self.assertEqual(crc16(b'123456789'), 0x29b1)
        for index in range(len(good)):
            bad = bytearray(good);bad[index] ^= 1
            with self.assertRaises(ValueError):
                decode(bad, 7)
        bad_packets = [good[:-1], good + b'\0', status(boot=0)]
        for offset, value in ((0, 0), (2, 2), (3, 6)):
            bad = bytearray(good);bad[offset] = value;bad_packets.append(seal(bad))
        for bad in bad_packets:
            with self.assertRaises(ValueError):
                decode(bad, 7)
        telemetry = TransportTelemetry()
        for bad in (good[:-1], status(boot=0), good[:-1] + bytes([good[-1] ^ 1])):
            self.assertIsNone(telemetry.observe(rx(bad), 1))
        self.assertEqual(telemetry.malformed['7'], 3)
        self.assertIsNone(telemetry.observe(rx(good).replace(b' 70 ', b' 69 '), 1))
        self.assertEqual(telemetry.malformed['7'], 4)

    def test_type6_accounting_survives_status_and_boot_changes(self):
        t = TransportTelemetry()
        t.observe(rx(pulse(4)), 1)
        t.observe(rx(pulse(6)), 2)
        first = t.observe(rx(status(1)), 3)
        self.assertEqual(first['receiver_type6']['received'], 2)
        self.assertEqual(first['receiver_type6']['forward_gaps'], 1)
        t.observe(rx(pulse(7)), 4)
        t.observe(rx(status(1, boot=456)), 5)  # Other status cannot reset stream.
        last = t.observe(rx(status(3)), 6)  # One missed status preserves counters.
        self.assertEqual(last['receiver_type6']['received'], 3)
        self.assertEqual(last['receiver_type6']['forward_gaps'], 1)
        self.assertEqual(last['receiver_type7']['forward_gaps'], 1)
        self.assertEqual(last['generated'], first['generated'])
        t.observe(rx(pulse(6)), 7);t.observe(rx(pulse(7)), 8)
        state = t.streams['0000002a:000000000000007b']['type6']
        self.assertEqual((state['last_sequence'], state['non_forward']), (7, 2))
        t.observe(rx(pulse(0xffffffff, boot=456)), 9)
        t.observe(rx(pulse(0, boot=456)), 10)
        state = t.streams['0000002a:00000000000001c8']['type6']
        self.assertEqual((state['received'], state['forward_gaps']), (2, 0))
        t.observe(b'STAT rx=10 qdrop=2 badlen=3 q=4\n', 11)
        last = t.observe(rx(status(4)), 12)
        self.assertEqual(last['receiver_status']['qdrop'], 2)
        self.assertEqual(last['receiver_status']['received_epoch'], 11)
        self.assertEqual(decode(pulse(1), 6)['dt_us'], 40000)
        broken = pulse(1)[:-1]
        self.assertIsNone(t.observe(rx(broken), 13))
        self.assertEqual(t.malformed['6'], 1)

    def test_type5_and_source_regression(self):
        # Real Type-5 packed layout still decodes and is preserved byte-for-byte.
        data = seal(MOVEMENT_WIRE.pack(0x4952, 1, 5, 1,
                    123, 100000, 1, 1, 0, 0, 0, 0, 0, 0, 0, 9652, 9652, 6, 0, 1000, 0))
        self.assertEqual(decode_movement(data)['completed_pulses'], 1)
        out, t = io.BytesIO(), TransportTelemetry()
        record_line(out, rx(data), 1, t)
        self.assertEqual(out.getvalue(), b'1.000000 ' + rx(data))
        self.assertEqual(t.streams, {})
        baseline = subprocess.check_output(['git', 'show', '65210f3:' + str(
            SKETCH.relative_to(ROOT) / 'IR_SCOPE_ESPNOW_TX.ino')], cwd=ROOT, text=True)
        self.assertEqual(function(SOURCE, 'sampler('), function(baseline, 'sampler('))
        self.assertEqual(function(SOURCE, 'radioSend('), function(baseline, 'radioSend('))
        for path in ['firmware/common/IrMovementDetector.h', 'firmware/common/IrMovementContract.h',
                     'firmware/common/IrMovementWire.h', str(SKETCH.relative_to(ROOT) / 'PulseEventEvidence.h')]:
            self.assertEqual((ROOT / path).read_bytes(), subprocess.check_output(
                ['git', 'show', '65210f3:' + path], cwd=ROOT))
        radio = function(SOURCE, 'radio(void*)')
        self.assertLess(radio.index('xQueueReceive(pulseEventQueue'), radio.index('servicePulseTransportStatus()'))
        self.assertLess(radio.index('xQueueReceive(movementQueue'), radio.index('servicePulseTransportStatus()'))


if __name__ == '__main__':
    unittest.main()
