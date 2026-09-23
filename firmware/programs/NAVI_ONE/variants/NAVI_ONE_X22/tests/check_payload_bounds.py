#!/usr/bin/env python3
"""Bound every telemetry record in the sketch against its buffer and the transport.

Written after the first X19 flash halted on

    [BOOT] FATAL: boot record oversize (874 bytes) - halting

because the boot record had been rewritten with policy spelled out in English
and never re-measured. The excursion record HAD been bounded, by hand, in the
same session -- so the lesson is not "count more carefully", it is that a hand
count does not survive the next edit. This runs in the test suite.

Every record must satisfy two limits:
  * its own buffer, or snprintf truncates and the JSON is malformed;
  * PubMsg::payload, or pub()'s strlcpy truncates it on the wire even when the
    local buffer was big enough.

Widths are worst case for the C type, except where the value is physically
bounded on this locomotive (an ADC reading cannot need eleven digits).
"""
import re
import sys
import os

SRC = os.path.join(os.path.dirname(__file__), "..", "NAVI_ONE_X22.ino")
TRANSPORT = 704          # sizeof(PubMsg::payload)

# Worst-case rendered width per conversion.
WIDTH = {
    "%lu":  10,   # uint32 millis/counters
    "%llu": 20,
    "%lld": 20,
    "%ld":  11,   # int32, worst case; ADC-derived ones are far smaller
    "%d":   11,
    "%u":   10,
    "%c":   1,
    "%.2f": 12,
    "%.3f": 14,
    "%f":   20,
}
SPEC = re.compile(r"%(?:lld|llu|lu|ld|\.\d+f|[dufsclx])")
DEFAULT_STR = 24         # an unknown %s argument

# Arguments whose range is bounded by the hardware or the map, asserted by
# inspection. Anything not listed gets its C type's worst case, which is the
# safe direction: the checker then demands a runtime guard instead.
BOUNDED = {
    "actualPwm": 4, "o.pwm": 4, "d.pwm": 4, "j->pwmAtDetect": 4,
    "j.pwmAtDetect": 4, "pwm": 4, "o.stepMs": 6,
    "motorDirection": 2, "estopped": 2, "lowVoltage": 2, "autoRunning": 2,
    "autoEnrolled": 2, "inaReady": 2, "positionKnown": 2, "mayAdapt": 2,
    "s.navMm": 4, "s.target": 4, "ns.navMm": 4, "o.offset": 5,
    "estMmPerS": 6, "brakeValue": 4, "rampTarget": 5, "SEQ_N": 4, "NAVI_BASELINE_ADAPT_PWM": 4,
    # every ADC-derived level: 0..4095, plus a sign
    "localRef": 6, "restRef": 6, "reference": 6, "shadowRef": 6,
    "rawAtDetect": 6, "departAtDetect": 6, "baseline": 6, "raw": 6,
    "shadowBaseline": 6, "peakCounts": 6, "peakSigned": 6, "primeValue": 6,
    "primeSpread": 6, "excursionSum": 9,
}


def bounded(arg):
    for key, w in BOUNDED.items():
        if key in arg:
            return w
    return None



def macros(text):
    out = {}
    for m in re.finditer(r'#define\s+(\w+)\s+"([^"]*)"', text):
        out[m.group(1)] = m.group(2)
    return out


def literal_of(call):
    """Concatenated format string of a snprintf call."""
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', call)
    return "".join(parts).replace('\\"', '"').replace("\\n", "\n").replace("\\\\", "\\")


def arg_list(call):
    """Everything after the format string, split at top-level commas."""
    depth, i, n = 0, 0, len(call)
    # skip to the end of the last string literal
    last = 0
    for m in re.finditer(r'"(?:[^"\\]|\\.)*"', call):
        last = m.end()
    tail = call[last:]
    args, cur = [], ""
    for ch in tail:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            if depth == 0:
                break
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        args.append(cur.strip())
    return [a for a in args if a]


def main():
    text = open(SRC).read()
    mac = macros(text)

    # Buffers are resolved by POSITION, not by name: half a dozen scopes in
    # this sketch each declare their own `char b[...]`, and a global name map
    # silently checks one record against another's buffer.
    decls = []
    for m in re.finditer(r"char\s+(\w+)\s*\[\s*([^\]]+?)\s*\]\s*;", text):
        size = m.group(2).strip()
        size = TRANSPORT if size == "sizeof(PubMsg::payload)" else size
        decls.append((m.start(), m.group(1), size))

    def buffer_at(name, pos):
        best = None
        for at, nm, size in decls:
            if nm == name and at < pos:
                best = size
        return best

    bad = 0
    checked = 0
    for m in re.finditer(r"snprintf\s*\(\s*(\w+)\s*,\s*sizeof\(\1\)\s*,", text):
        name = m.group(1)
        # take the whole call by balancing parentheses from the opening one
        i = text.index("(", m.start())
        depth, j = 0, i
        while j < len(text):
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        call = text[i:j]
        fmt = literal_of(call)
        args = arg_list(call)
        specs = SPEC.findall(fmt)
        literal = len(SPEC.sub("", fmt))

        total = literal
        ai = 0
        for sp in specs:
            a = args[ai] if ai < len(args) else ""
            ai += 1
            if sp == "%s":
                key = a.strip()
                total += len(mac[key]) if key in mac else DEFAULT_STR
            else:
                b = bounded(a)
                total += b if b is not None else WIDTH.get(sp, 12)

        raw = buffer_at(name, m.start())
        try:
            cap = int(raw)
        except (TypeError, ValueError):
            print("  ?  %-12s buffer size %r not resolved - skipped" % (name, raw))
            continue

        # A record is safe if it PROVABLY fits, or if its call site tests
        # snprintf's return value and refuses to enqueue a truncated payload.
        # The second is what the 2026-08-29 alert incident taught; the first is
        # what the boot record needed, because there a guard means the
        # locomotive halts.
        window = text[m.start() - 120: m.start()]
        guarded = bool(re.search(r"(const\s+)?int\s+\w+\s*=\s*$", window.strip() + "=")) or \
                  bool(re.search(r"=\s*snprintf", text[max(0, m.start() - 40):m.start() + 10]))
        # A GUARD THAT HALTS IS NOT A SAFETY NET. The boot record's guard
        # stops the locomotive rather than dropping a message, so that record
        # must be PROVED to fit; being guarded earns it nothing. This is the
        # distinction the first X19 flash turned on: the guard worked exactly
        # as designed and the locomotive still went online and then stale.
        halting = "for(;;)" in text[j: j + 400] or "for (;;)" in text[j: j + 400]
        checked += 1
        limit = min(cap, TRANSPORT)
        ok = total < limit or (guarded and not halting)
        if not ok:
            bad += 1
        line = text.count("\n", 0, m.start()) + 1
        print("  %s %-6s line %-5d worst case %4d   buffer %4d%s%s"
              % ("ok " if ok else "FAIL", name, line, total, cap,
                 "   [halts if over]" if guarded and halting
                 else ("   [guarded]" if guarded else ""),
                 "" if ok else "   <<< OVER and UNGUARDED (transport %d)" % TRANSPORT))

    print("\n%d records checked, %d over limit" % (checked, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
