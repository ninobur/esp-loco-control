#!/usr/bin/env python3
"""test_quorum_trace_authority.py — source audit of QUORUM.ino's trace
instrumentation and QuorumTrace.h.

INVESTIGATORY / UNAPPROVED.

QuorumTrace.h's ring/batch engine is exercised for real by
test_quorum_trace.cpp (g++). What that test CANNOT show is that the trace
hooks embedded in QUORUM.ino itself -- an Arduino sketch, not host-buildable
without the ESP32 core -- behave correctly and safely. This audits the
SOURCE the same way test_no_control_authority.py already does for
HALL_WAVEFORM_TEST.ino: by finding function bodies and call sites and
checking what is and is not in them.

Static analysis, not proof of runtime behaviour -- it cannot see through a
function pointer, and it is not a substitute for reading the diff. It does
catch the failure that actually happens: a trace hook that grew a write to
control state, or a call site that stopped using the QT_* macro.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.join(HERE, "..", "QUORUM.ino")
TRACE_H = os.path.join(HERE, "..", "QuorumTrace.h")

failures = 0
checks = 0


def ck(cond, what, detail=""):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  %s%s" % (what, ("\n        " + detail) if detail else ""))


# --- source analysis helpers (same technique as test_no_control_authority.py,
#     reimplemented here rather than imported: the two sketches are audited
#     independently on purpose, so a change to one script cannot silently
#     weaken the other) -----------------------------------------------------

def strip_noncode(src):
    """Blank out comments and string/char literals, preserving offsets."""
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
    r"(?:^|\n)(?:[A-Za-z_][\w\s\*&:<>,]*?\b(?P<name>\w+)\s*\([^;{}]*\))\s*\{")


def functions(code):
    out = {}
    for m in DEF_RE.finditer(code):
        name = m.group("name")
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


def full_definitions(code):
    """Like functions(), but keyed span covers the SIGNATURE too (from the
    'static'/return-type start through the closing brace), not just the
    {...} body. functions() deliberately returns body-only so content
    checks ("does this function's body contain X") don't also match the
    function's own name in its signature; stripping definitions to hunt
    for stray call sites needs the wider span, or a function's own
    signature (e.g. "static void qtFoo(...)" right before its body) reads
    as a call to qtFoo() once the body is removed."""
    out = {}
    for m in DEF_RE.finditer(code):
        name = m.group("name")
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
        out[name] = code[m.start():j + 1]
    return out


def strip_comments_only(src):
    """Like strip_noncode, but leaves string/char literals intact -- for the
    few checks that need to see an actual string literal's content. Braces
    inside a literal are passed through unstripped too, which is safe here:
    none of this file's string literals contain '{' or '}'."""
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
                if src[i] == '\\' and i + 1 < n:
                    i += 2
                else:
                    i += 1
            if i < n:
                i += 1
        else:
            i += 1
    return "".join(out)


# Forward-declaration prototypes ("static void qtFoo(...);") end in ';', not
# '{', so functions() correctly never captures them as bodies -- but they are
# STILL text of the form "name(" and must not be counted as call sites.
PROTOTYPE_RE = re.compile(
    r"(?:^|\n)\s*static\s+[\w<>:&\*\s]+?\b\w+\s*\([^;{}]*\)\s*;")


def strip_prototypes(code):
    return PROTOTYPE_RE.sub("", code)


def main():
    print("QUORUM TRACE — source authority audit (investigatory)\n")
    raw = open(SKETCH).read()
    src = strip_noncode(raw)
    trace_raw = open(TRACE_H).read()
    trace_src = strip_noncode(trace_raw)
    fns = functions(src)
    fns_full = full_definitions(src)   # signature + body, for the stray-call scan only
    fns_raw = functions(strip_comments_only(raw))   # same boundaries, string literals intact
    ck(len(fns) > 30, "the source parser found the sketch's functions",
       "parsed only %d: %s" % (len(fns), ", ".join(sorted(fns))))

    QT_FUNCS = ["qtTraceBegin", "qtSendUdp", "qtSampleTick",
                "qtDecisionEventOpened", "qtDecisionEventFloorReject",
                "qtDecisionEventClosed", "qtDecisionNavEntry",
                "qtDecisionGateResult", "qtDecisionAccept",
                "qtDecisionNavState", "qtDecisionQuorum",
                "qtDirMirrorUpdate", "qtStatusService", "qtNetworkDrain"]
    for f in QT_FUNCS:
        ck(f in fns, "%s was found and parsed" % f)

    # ------------------------------------------------------- requirement 1
    # "Trace disabled produces no trace calls ... beyond compile-time
    # elimination." Every QT_* macro must expand to nothing in the #else
    # branch, and every call site in the sketch must go through a macro
    # (QT_UPPER_CASE), never a bare qtLowerCase(...) call -- the latter
    # would not compile out when QUORUM_TRACE is undefined.
    print("\ncompile-time elimination (requirement 1)")
    noop_re = re.compile(
        r"#define\s+(QT_[A-Z_]+)\([^)]*\)\s*do\s*\{\s*\}\s*while\s*\(\s*0\s*\)")
    noop_macros = set(noop_re.findall(trace_raw))
    expect_macros = {"QT_TRACE_BEGIN", "QT_SAMPLE_TICK", "QT_DECISION_EVENT_OPENED",
                      "QT_DECISION_EVENT_FLOOR_REJECT", "QT_DECISION_EVENT_CLOSED",
                      "QT_DECISION_NAV_ENTRY", "QT_DECISION_GATE_RESULT",
                      "QT_DECISION_ACCEPT", "QT_DECISION_NAV_STATE",
                      "QT_DECISION_QUORUM", "QT_DIR_MIRROR_UPDATE",
                      "QT_STATUS_SERVICE", "QT_NETWORK_DRAIN"}
    missing = expect_macros - noop_macros
    ck(not missing, "every QT_* macro has a do{}while(0) fallback when QUORUM_TRACE is off",
       "missing a no-op definition: %s" % ", ".join(sorted(missing)))

    # Every call site of a qt* trace function in the sketch, outside the
    # QUORUM TRACE (definitions) block itself, must be reached only via its
    # QT_* macro -- a direct qtSampleTick(...) call would survive an OFF
    # build's preprocessing and fail to link (undefined reference), so this
    # is also a cheap correctness check, not just a style one.
    direct_call_re = re.compile(r"(?<![\w#])(" + "|".join(QT_FUNCS) + r")\s*\(")
    # Calls inside the functions' OWN bodies (e.g. qtNetworkDrain calling
    # qtSendUdp) are fine -- those only exist when QUORUM_TRACE is already
    # on. Filter those out by removing the function bodies, and remove the
    # forward-declaration prototypes (which are declarations, not calls).
    src_without_qt_bodies = src
    for f in QT_FUNCS:
        if f in fns_full:
            src_without_qt_bodies = src_without_qt_bodies.replace(fns_full[f], " ")
    src_without_qt_bodies = strip_prototypes(src_without_qt_bodies)
    stray_calls = direct_call_re.findall(src_without_qt_bodies)
    ck(not stray_calls,
       "no call site reaches a qt* function directly, bypassing its QT_* macro",
       "found direct call(s): %s" % ", ".join(stray_calls))

    # ------------------------------------------------------- requirement 2
    print("\nno additional ADC read (requirement 2)")
    for name, body in ((f, fns[f]) for f in QT_FUNCS if f in fns):
        ck("analogRead" not in body, "%s contains no analogRead() call" % name)
    ck("analogRead" not in trace_src, "QuorumTrace.h contains no analogRead() call")
    ck(src.count("analogRead(") == 1,
       "the sketch calls analogRead() exactly once, total (in readAveragedADC)",
       "found %d call sites" % src.count("analogRead("))

    # ------------------------------------------------------- requirement 3
    # "Hall task must never perform UDP, MQTT, Serial formatting, dynamic
    # allocation or blocking I/O." qtSampleTick and the three detectorSample-
    # side decision hooks (event opened/floor-reject/closed) all run on
    # hallTask; qtDecisions.push()/qtSamples.addSample() are the only calls
    # they may reach beyond plain arithmetic.
    print("\nHall task performs no transport/formatting/allocation/blocking (requirement 3)")
    HALLTASK_SIDE = ["qtSampleTick", "qtDecisionEventOpened",
                     "qtDecisionEventFloorReject", "qtDecisionEventClosed"]
    FORBIDDEN = [
        ("qtUdp", "a direct UDP call"), ("mqtt.", "a direct MQTT call"),
        ("Serial.", "a Serial call"), ("malloc", "dynamic allocation"),
        ("new ", "dynamic allocation (new)"), ("snprintf", "string formatting"),
        ("sprintf", "string formatting"), ("delay(", "a blocking delay"),
        ("vTaskDelay", "a blocking delay"),
    ]
    for name in HALLTASK_SIDE:
        body = fns.get(name, "")
        ck(bool(body), "%s was found for the requirement-3 audit" % name)
        for token, what in FORBIDDEN:
            ck(token not in body, "%s contains no %s" % (name, what))

    # detectorSample() and hallTask() themselves: confirm the actual call
    # sites are QT_SAMPLE_TICK / QT_DECISION_EVENT_* (the macro, not a raw
    # analogRead or transport call added directly at the hook point).
    detector_body = fns.get("detectorSample", "")
    halltask_body = fns.get("hallTask", "")
    ck("QT_DECISION_EVENT_OPENED" in detector_body, "detectorSample() hooks event-open via the macro")
    ck("QT_DECISION_EVENT_FLOOR_REJECT" in detector_body, "detectorSample() hooks the floor-reject via the macro")
    ck("QT_DECISION_EVENT_CLOSED" in detector_body, "detectorSample() hooks event-close via the macro")
    ck("QT_SAMPLE_TICK" in halltask_body, "hallTask() hooks the sample tick via the macro")
    for token, what in FORBIDDEN:
        ck(token not in detector_body or token in ("QT_",),
           "detectorSample() itself contains no %s" % what)

    # ------------------------------------------------------- requirement 8
    print("\ntrace cannot write motor/nav/estop/command state (requirement 8)")
    CONTROL_WRITE = re.compile(
        r"(?<![.>\w])(commandedPwm|actualPwm|estopped|navMm|navState|navDir|"
        r"motorDirection|sessionDir|autoRunning|autoEnrolled)\s*(=[^=]|\+\+|--)")
    for name in QT_FUNCS:
        body = fns.get(name, "")
        bad = CONTROL_WRITE.search(body)
        ck(not bad, "%s writes no motor/navigation/command state" % name,
           bad.group(0) if bad else "")
        ck("digitalWrite" not in body and "ledcWrite" not in body and "pwmWriteCompat" not in body,
           "%s contains no direct motor pin write" % name)
        ck("requestPwm" not in body and "applyDirection" not in body and
           "navDeclare" not in body and "acceptEvent(" not in body and
           "navOnMarker(" not in body,
           "%s does not call back into any control-path function" % name)
    # qtDirMirrorUpdate is the one function ALLOWED to touch the trace-only
    # mirror; confirm it writes ONLY that, never navDir itself.
    mirror_body = fns.get("qtDirMirrorUpdate", "")
    ck("navDir=" not in mirror_body.replace(" ", "") and "navDir =" not in mirror_body,
       "qtDirMirrorUpdate() writes the trace-only mirror, never navDir itself")
    ck("qtNavDirMirror" in mirror_body, "qtDirMirrorUpdate() does write its own trace-only mirror")

    # ---------------------------------------------- requirement 7 (partial)
    # Full end-to-end proof needs a host-runnable QUORUM navigation core,
    # which does not exist in this repository (see the QUORUM nav audit doc)
    # -- this is a scope boundary, not an oversight. What IS checkable
    # statically: the exact fields the spec names are actually assigned,
    # unconditionally, at the two call sites a wrong-polarity accepted event
    # passes through. test_quorum_trace.cpp separately proves the wire
    # format/ring carries a record built with these fields intact.
    print("\nwrong-polarity accept evidence fields are populated (requirement 7, static half)")
    accept_body = fns.get("qtDecisionAccept", "")
    for field in ("navMmBefore", "navMmAfter", "observedPolarity", "ringInserted"):
        ck(("d.%s" % field) in accept_body, "qtDecisionAccept() assigns %s" % field)
    ck("d.ringInserted=1" in accept_body.replace(" ", ""),
       "qtDecisionAccept() always marks ringInserted=1 (matches acceptEvent()'s "
       "unconditional pushRing(), per the QUORUM nav audit)")
    navstate_body = fns.get("qtDecisionNavState", "")
    navstate_body_raw = fns_raw.get("qtDecisionNavState", "")   # string literals intact
    ck('"DISAGREE"' in navstate_body_raw, "qtDecisionNavState() classifies the DISAGREE event")
    ck("d.expectedPolarity=dnaAt(navMm)" in navstate_body.replace(" ", ""),
       "qtDecisionNavState() assigns expectedPolarity from the map, not a guess")
    ck("d.observedPolarity" in navstate_body, "qtDecisionNavState() assigns observedPolarity")

    # -------------------------------------------------------- requirement 10
    print("\nToby/Otto config semantics unchanged (requirement 10)")
    for profile in ("LL_LocoConfig_9950011.h", "LL_LocoConfig_9950012.h"):
        path = os.path.join(HERE, "..", profile)
        text = open(path).read()
        ck("QUORUM_TRACE" not in text and "QtSample" not in text and "qt" not in text.lower(),
           "%s carries no trace-related symbol (config semantics untouched)" % profile)

    # ----------------------------------------------------------- boot identity
    print("\nboot identity")
    ck('"QUORUM_1_12C"' in raw, "sketch identity string present")
    ck("NO NAVIGATION OR MOTOR AUTHORITY" in raw,
       "the trace section states its own limit in the sketch itself")
    ck("NO NAVIGATION OR MOTOR AUTHORITY" in trace_raw,
       "QuorumTrace.h states the same limit")

    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
