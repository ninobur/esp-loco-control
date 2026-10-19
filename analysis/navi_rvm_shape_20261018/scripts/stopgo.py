"""STOP/GO WaveformPause, Python reference of firmware RvmPause.h.
STOPPED = the highest applied PWM among a block's reads < 15 (operator ruling
2026-10-18; the first ruling was < 25). Such a block is not
appended to the recognition waveform, except from a declaration until the first
moving block (the declared stationary starting baseline). Empty blocks carry PWM 0.
The recognizer itself (rvm_local) is unchanged: it runs on the admitted blocks,
and positions are mapped back to tracing block ids."""
import numpy as np
from common import *

STOPPED_BELOW_PWM = 15


def block_pwm(n_blocks):
    """Highest applied PWM per 20 ms block of boot 1 (same block grid as s01)."""
    N = np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
    a = np.memmap(HG/f'native_{BOOT1}.bin', mode='r', dtype=N)
    us = a['us'].astype(np.int64); B = DT_MS * 1000; b = (us - us[0] // B * B) // B
    out = np.zeros(max(n_blocks, int(b[-1]) + 1), np.int16)
    np.maximum.at(out, b, a['pwm'].astype(np.int16))
    return out[:n_blocks]


def firmware_mask(h, pwm, declarations, stopcmd=None):
    """Firmware RvmPause.h (current): PWM pause, pattern-based resumption, and the
    arrival mirror on the commanded-stop hold-back. Startup handling included."""
    import continuation as K
    if stopcmd is None: stopcmd = K.commanded_stop_proxy(pwm)
    return K.pause_sequence(np.asarray(h, float), np.asarray(pwm), stopcmd, declarations)


def resumption_mask(h, pwm, declarations):
    """Firmware RvmPause.h: the PWM pause plus pattern-based resumption after GO
    (continuation._rule, two consecutive blocks outside the stationary range)."""
    import continuation as K
    return K._rule(np.asarray(h, float), admit_mask(pwm, declarations), 2, declarations=declarations)


def admit_mask(pwm, declarations):
    """declarations: block indices at which the operator declared (startup)."""
    keep = np.zeros(len(pwm), bool); startup = False; decl = set(int(d) for d in declarations)
    for i, p in enumerate(pwm):
        if i in decl: startup = True
        if p >= STOPPED_BELOW_PWM: startup = False; keep[i] = True
        else: keep[i] = startup
    return keep


class Compressed:
    """The admitted waveform and the map back to block ids."""
    def __init__(self, h, ccw, keep):
        self.ids = np.where(keep)[0]; self.h = h[keep]; self.ccw = ccw[keep]
        self.pos = np.cumsum(keep) - keep          # admitted blocks before each block id

    def to_ids(self, x):
        """Waveform position(s) -> block id(s), linear between admitted blocks."""
        return np.interp(np.asarray(x, float), np.arange(len(self.ids)), self.ids)

    def start_of(self, block_id):
        return int(self.pos[block_id])
