#!/usr/bin/env python3
"""Decode IR transport diagnostics; preserve raw Type-6 evidence separately.

Only Python's standard library is required. Counts are recorder-process-local;
sequence state is independent for each (sid, boot) and each packet type.
"""
import binascii
import struct

STATUS_WIRE = struct.Struct('<HBBIIQ12IH')
STATUS_FIELDS = ('magic version type sid sequence boot generated sent failed dropped '
                 'logDropped queueDepth queueHigh logQueueDepth lagMaxUs timeouts '
                 'busyDrops sendErrors crc').split()
PULSE_WIRE = struct.Struct('<HBBIIQQQQQIBHH')
PULSE_FIELDS = ('magic version type sid sequence boot completed completed_us dt_us '
                'nominal_um pitch_um reason span crc').split()
assert STATUS_WIRE.size == 70 and PULSE_WIRE.size == 61


def crc16(data):
    return binascii.crc_hqx(data, 0xffff)


def decode(data, packet_type):
    wire, fields = {6: (PULSE_WIRE, PULSE_FIELDS),
                    7: (STATUS_WIRE, STATUS_FIELDS)}[packet_type]
    if len(data) != wire.size:
        raise ValueError('Type-%d length' % packet_type)
    row = dict(zip(fields, wire.unpack(data)))
    if (row['magic'], row['version'], row['type']) != (0x4952, 1, packet_type):
        raise ValueError('Type-%d schema' % packet_type)
    if crc16(data[:-2]) != row['crc']:
        raise ValueError('Type-%d CRC' % packet_type)
    if not row['boot']:
        raise ValueError('zero movement boot')
    if packet_type == 6 and (not row['completed'] or not row['completed_us'] or
                            row['pitch_um'] != 9652 or row['reason'] > 6 or
                            row['nominal_um'] != row['completed'] * row['pitch_um']):
        raise ValueError('Type-6 evidence contract')
    # Do not reject a status counter discrepancy: readings can race or wrap.
    row['boot'] = '%016x' % row['boot']
    return row


def account_sequence(state, sequence):
    state['received'] += 1
    if state['last_sequence'] is None:
        state['first_sequence'] = sequence
        state['last_sequence'] = sequence
        return
    step = (sequence - state['last_sequence']) & 0xffffffff
    if 0 < step < 0x80000000:
        state['forward_gaps'] += step - 1
        state['last_sequence'] = sequence
    else:
        # Preserve duplicate/late evidence without moving the high-water back.
        # Forward gaps are discontinuities, not repaired estimates of RF loss.
        state['non_forward'] += 1


def sequence_state():
    return dict(received=0, first_sequence=None, last_sequence=None,
                forward_gaps=0, non_forward=0)


class TransportTelemetry:
    def __init__(self):
        self.streams = {}
        self.malformed = {'6': 0, '7': 0, 'rx_envelope': 0}
        self.receiver_status = None

    def observe(self, line, received_epoch):
        """Return a Type-7 log row, or None. Never modify the raw serial line."""
        fields = line.split()
        if not fields:
            return None
        if fields[0] == b'STAT':
            try:
                values = {k.decode('ascii'): int(v) for k, v in
                          (part.split(b'=', 1) for part in fields[1:])}
                self.receiver_status = dict(received_epoch=received_epoch, **values)
            except (ValueError, UnicodeError):
                pass  # Unrecognized diagnostics remain intact in the raw file.
            return None
        if fields[0] != b'RX':
            return None
        packet_type = None
        try:
            if len(fields) != 6:
                raise ValueError('RX envelope fields')
            data = bytes.fromhex(fields[5].decode('ascii'))
            if len(data) < 4 or data[:2] != b'RI' or data[3] not in (6, 7):
                return None
            packet_type = data[3]
            rx_ms, rssi, length = map(int, fields[1:4])
            if length != len(data) or crc16(data) != int(fields[4], 16):
                raise ValueError('RX envelope length/CRC')
            row = decode(data, packet_type)
        except (ValueError, UnicodeError):
            self.malformed[str(packet_type) if packet_type else 'rx_envelope'] += 1
            return None
        key = '%08x:%s' % (row['sid'], row['boot'])
        stream = self.streams.setdefault(key, {'type6': sequence_state(),
                                               'type7': sequence_state()})
        account_sequence(stream['type%d' % packet_type], row['sequence'])
        if packet_type == 6:
            return None
        row.update(received_epoch=received_epoch, receiver_ms=rx_ms, rssi=rssi,
                   authority='DIAGNOSTIC_ONLY',
                   receiver_type6=dict(stream['type6']),
                   receiver_type7=dict(stream['type7']),
                   receiver_status=self.receiver_status)
        stream['last_status'] = row
        return row

    def health(self):
        return dict(accounting_scope='recorder_process; per sid/boot/type',
                    malformed=self.malformed, streams=self.streams,
                    receiver_status=self.receiver_status)


def record_line(stream, line, received_epoch, telemetry):
    """Write original evidence first, then an additive, identifiable JSON row."""
    prefix = ('%.6f ' % received_epoch).encode('ascii')
    raw = prefix + line
    stream.write(raw)
    row = telemetry.observe(line, received_epoch)
    if row is not None:
        import json
        # A timeout can return a partial serial line without a trailing LF.
        # Keep the original bytes and separate the derived record if necessary.
        record = (b'' if line.endswith(b'\n') else b'\n') + prefix
        record += b'PULSE_TRANSPORT ' + json.dumps(row, sort_keys=True).encode('ascii') + b'\n'
        stream.write(record)
        return len(raw) + len(record)
    return len(raw)
