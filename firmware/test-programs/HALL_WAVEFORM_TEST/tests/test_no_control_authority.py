#!/usr/bin/env python3
"""test_no_control_authority.py — source audit of HALL_WAVEFORM_TEST.ino.

INVESTIGATORY / UNAPPROVED.

The instrument's central claim is that the recorder cannot move the
locomotive. This checks it the way this project has checked it before: by
enumerating every motor-write call site and naming which function each one is
in. A motor write anywhere outside the small allowlist fails the audit.

It also checks that the deleted Module C navigation behaviour really is gone,
and that the Hall reading path contains no rejection, averaging or
classification that could quietly become policy.

Static analysis, not proof of runtime behaviour — it cannot see through a
function pointer, and it is not a substitute for reading the diff. It does
catch the failure that actually happens: a motor write added to the recorder
during a later edit.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.join(HERE, "..", "HALL_WAVEFORM_TEST.ino")
CAPTURE = os.path.join(HERE, "..", "HallCapture.h")

failures = 0
checks = 0


def ck(cond, what, detail=""):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  %s%s" % (what, ("\n        " + detail) if detail else ""))


# --- source analysis helpers -----------------------------------------------
# Everything below audits CODE, never comments: this file's own prose lists the
# deleted Module C symbols, and a naive grep would find them there and either
# fail forever or, worse, pass because a real call sat inside a comment.

def strip_noncode(src):
    """Blank out comments and string/char literals, preserving offsets so byte
    positions still line up with the original text."""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            while i < n and src[i] != '\n':
                out[i] = ' '; i += 1
        elif c == '/' and i + 1 < n and src[i + 1] == '*':
            out[i] = out[i + 1] = ' '; i += 2
            while i < n and not (src[i] == '*' and i + 1 < n and src[i + 1] == '/'):
                if src[i] != '\n':
                    out[i] = ' '
                i += 1
            if i < n:
                out[i] = ' '
                if i + 1 < n:
                    out[i + 1] = ' '
                i += 2
        elif c in '"\'':
            quote = c
            i += 1
            while i < n and src[i] != quote:
                if src[i] == '\\':
                    out[i] = ' '; i += 1
                    if i < n and src[i] != '\n':
                        out[i] = ' '
                elif src[i] != '\n':
                    out[i] = ' '
                i += 1
            if i < n:
                i += 1
        else:
            i += 1
    return "".join(out)


DEF_RE = re.compile(
    r"(?:^|\n)(?:BLYNK_WRITE\(\s*(?P<vpin>\w+)\s*\)|"
    r"[A-Za-z_][\w\s\*&:<>,]*?\b(?P<name>\w+)\s*\([^;{}]*\))\s*\{")


def functions(code):
    """name -> function body, code only. Brace-matched, so nested blocks and
    single-line definitions both come out whole."""
    out = {}
    for m in DEF_RE.finditer(code):
        name = m.group("vpin") or m.group("name")
        if name in ("if", "for", "while", "switch", "else", "struct", "__attribute__"):
            continue
        i = code.index("{", m.end() - 1)
        depth, j = 0, i
        while j < len(code):
            if code[j] == "{":
                depth += 1
            elif code[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        out[name] = code[i:j + 1]
    return out


def main():
    print("HALL_WAVEFORM_TEST — control-authority audit (investigatory)\n")
    raw = open(SKETCH).read()
    src = strip_noncode(raw)
    cap_raw = open(CAPTURE).read()
    cap = strip_noncode(cap_raw)
    fns = functions(src)
    ck(len(fns) > 15, "the source parser found the sketch's functions",
       "parsed only: %s" % ", ".join(sorted(fns)))

    # ---------------------------------------------------------------- motor
    # Every write to the motor, and the only functions allowed to make one.
    MOTOR_WRITE = re.compile(r"\b(ledcWrite\s*\(\s*MOTOR_PWM_PIN|digitalWrite\s*\(\s*MOTOR_DIR_PIN)")
    ALLOWED = {
        "setup",                      # boot at PWM 0, direction pin set once
        "serviceRamp",                # the one ramp stepper
        "engageEStop",                # E-STOP cuts PWM directly
        "applyCommand",               # operator DIR command
        "VPIN_DIRECTION",             # BLYNK_WRITE handler: operator direction
    }
    print("motor-write call sites")
    sites = []
    for fname, body in fns.items():
        for _m in MOTOR_WRITE.finditer(body):
            sites.append(fname)
    for fname in sorted(set(sites)):
        print("    %-28s %d write(s)  %s"
              % (fname, sites.count(fname), "OK" if fname in ALLOWED else "NOT ALLOWED"))
    stray = sorted(set(sites) - ALLOWED)
    ck(not stray, "no motor write outside the operator/E-STOP allowlist",
       "stray: %s" % ", ".join(stray))
    ck(sites, "the audit actually found the motor writes (guard against a "
              "parser that silently matches nothing)")

    # The recorder's own functions: no motor write, and no motor STATE write
    # either — not even a ramp target.
    RECORDER = ["samplerTask", "networkTask", "queueAux", "sendPacket",
                "sendAnchor", "sendStatus", "classifyAnn", "setAnnThresholds",
                "calibrateChannel", "mirrorSerial"]
    print("\nrecorder functions")
    # A write to the control state itself — not to a copy of it in an outgoing
    # status struct, which is a reading, not a command.
    TARGET_WRITE = re.compile(r"(?<![.>\w])(rampTarget|rampCurrent|manualDirection|"
                              r"localEStopEngaged|fixedTarget|fixedMode|seqRunning)\s*"
                              r"(=[^=]|\+\+|--)")
    for fname in RECORDER:
        if fname not in fns:
            continue
        body = fns[fname]
        bad_motor = MOTOR_WRITE.search(body)
        bad_state = TARGET_WRITE.search(body)
        print("    %-28s %s" % (fname, "clean" if not (bad_motor or bad_state) else "WRITES CONTROL STATE"))
        ck(not bad_motor, "%s does not write the motor" % fname)
        ck(not bad_state, "%s does not write control state" % fname,
           bad_state.group(0) if bad_state else "")

    ck("samplerTask" in fns, "the sampler task exists and was parsed")
    ck("networkTask" in fns, "the network task exists and was parsed")

    # ------------------------------------------------- deleted Module C code
    print("\ndeleted Module C behaviour")
    GONE = {
        "decodeBlock": "block decoding",
        "BLOCK_PATIO": "block identities",
        "myCurrentBlock": "block tracking",
        "isReverseLockedOut": "pre-acquisition reverse lockout",
        "isReversalOfLastPair": "reversal detection",
        "MODE_PROTECTED": "protected/ACC/AOP mode framework",
        "modeIsProtected": "mode gating",
        "esp_now_send": "ESP-NOW block/status broadcast",
        "MSG_TYPE_BLOCK": "dispatcher block packets",
        "serviceDetect": "magnet-pair navigation state machine",
        "SECOND_MAGNET_TIMEOUT_MS": "magnet-pair timing",
        "PAIR_LOCKOUT_MS": "pair lockout",
    }
    for token, what in sorted(GONE.items()):
        present = token in src
        print("    %-26s %s" % (what, "still present" if present else "gone"))
        ck(not present, "%s is absent from the diagnostic sketch" % what)

    # ------------------------------------------------- acquisition integrity
    print("\nacquisition integrity")
    sampler = fns.get("samplerTask", "")
    ck("analogRead(HALL_PIN_A)" in sampler, "the sampler reads the Hall pin")
    ck(sampler.count("analogRead") <= 2,
       "one acquisition per channel per slot — no averaging loop in the sampler")
    ck("readAveragedADC" not in src,
       "the base sketch's 12-read averaging function is not carried over")
    ck("vTaskDelayUntil" in sampler, "the sampler runs on a fixed grid, not a sleep")
    ck("esp_timer_get_time" in sampler, "samples are timestamped in microseconds")

    # The capture engine must contain no threshold on what is recorded.
    print("\ncapture engine")
    for token, what in (("threshold", "a recording threshold"),
                        ("if (raw", "an amplitude test"),
                        ("reject", "a rejection path")):
        present = token.lower() in cap.lower()
        print("    %-26s %s" % (what, "PRESENT" if present else "absent"))
        ck(not present, "the capture engine contains no %s" % what)
    add = cap.split("void addSample")[1].split("if (n_ >= HWT_BATCH_SAMPLES)")[0]
    ck("addSample" in cap and "return;" not in add,
       "addSample has no early return: it cannot refuse a sample")

    # --------------------------------------------------------- boot identity
    # These live in string literals and comments, so they are checked against
    # the raw file rather than the code-only text.
    print("\nboot identity")
    ck('"HALL_WAVEFORM_TEST_0_1"' in raw, "unique sketch name")
    ck("DIAGNOSTIC ONLY — NO NAVIGATION AUTHORITY" in raw, "banner states the limit")
    ck("INVESTIGATORY" in raw, "banner marks the build investigatory")
    ck(raw.count("DIAGNOSTIC ONLY — NO NAVIGATION AUTHORITY") >= 2,
       "the limit is in both the file header and the boot banner")

    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
