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


def extract_block(text, start_regex):
    """Find start_regex in text, then return the brace-matched {...} block
    that begins at the first '{' at or after the match. Same brace-counting
    technique as functions()/full_definitions(), applied to an arbitrary
    start point (e.g. one `else if` branch inside a larger function body)
    rather than a function signature. Returns '' if start_regex has no
    match."""
    m = re.search(start_regex, text)
    if not m:
        return ""
    i = text.index("{", m.end() - 1)
    depth, j = 0, i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                break
        j += 1
    return text[i:j + 1]


def trace_gated_mask(src):
    """One bool per line of src: True if that line sits inside a
    '#ifdef QUORUM_TRACE' block, at ANY nesting depth -- correct even when
    an unrelated #ifdef/#ifndef (e.g. QT_TRACE_HOST's include guard, or
    MISSION_ONLY_STATION) is nested inside or adjacent to it, because a
    nested condition can only make inclusion MORE restrictive, never less:
    the line is still fully absent whenever the outer QUORUM_TRACE is
    undefined. Assumes no '#else' is ever used directly on a
    QUORUM_TRACE-conditioned frame in this file -- verified separately,
    below, rather than assumed silently."""
    lines = src.split("\n")
    mask = [False] * len(lines)
    stack = []   # True = this frame IS the '#ifdef QUORUM_TRACE' test itself
    for idx, line in enumerate(lines):
        s = line.strip()
        mask[idx] = any(stack)
        if re.match(r"#\s*ifdef\s+QUORUM_TRACE\b", s):
            stack.append(True)
        elif re.match(r"#\s*(ifdef|ifndef|if)\b", s):
            stack.append(False)
        elif re.match(r"#\s*endif\b", s):
            if stack:
                stack.pop()
    return mask


def outside_trace(src):
    """src with every QUORUM_TRACE-gated line blanked -- an approximation
    of what the preprocessor leaves behind when QUORUM_TRACE is undefined,
    good enough to prove a symbol never appears there."""
    lines = src.split("\n")
    mask = trace_gated_mask(src)
    return "\n".join(("" if m else l) for l, m in zip(lines, mask))


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

    # --------------------------------------------- operator anchor mechanism
    # requirements 1-13 (ngr/loco/<id>/cmd/trace_anchor -- README_TRACE.md
    # "Anchor mechanism"). The anchor call sites use direct #ifdef wrapping,
    # not the QT_* macro layer QT_FUNCS' requirement-1 check models, so this
    # uses trace_gated_mask()/outside_trace() instead -- a different, also
    # rigorous, proof of the same "compiles out completely" property.
    print("\noperator anchor mechanism (requirements 1-13)")

    # The scanner's key assumption, checked rather than assumed: no '#else'
    # line ever appears while nested inside a QUORUM_TRACE frame in this
    # file today. If a future edit adds one, trace_gated_mask() would need
    # to model the flip and this check is what would catch the drift.
    stk, else_inside_trace = [], False
    for line in raw.split("\n"):
        s = line.strip()
        if re.match(r"#\s*ifdef\s+QUORUM_TRACE\b", s):
            stk.append(True)
        elif re.match(r"#\s*(ifdef|ifndef|if)\b", s):
            stk.append(False)
        elif re.match(r"#\s*else\b", s) and any(stk):
            else_inside_trace = True
        elif re.match(r"#\s*endif\b", s) and stk:
            stk.pop()
    ck(not else_inside_trace,
       "no #else appears inside a QUORUM_TRACE block (trace_gated_mask()'s assumption holds)")

    # requirement 1: compile-time absent with trace OFF.
    outside = outside_trace(raw)
    for sym in ("T_CMD_TRACE_ANCHOR", "qtSubmitAnchor", "qtPublishAnchorAck"):
        ck(sym not in outside,
           "%s never appears outside a QUORUM_TRACE-gated region in QUORUM.ino "
           "(requirement 1: anchor capability is compile-time absent with trace OFF)" % sym)
    trace_h_outside = (trace_raw[:trace_raw.index("#ifdef QUORUM_TRACE")] +
                       trace_raw[trace_raw.index("#endif // QUORUM_TRACE"):])
    for sym in ("QtAnchorRing", "QtAnchorTextDecision", "qtDecideAnchorText", "QT_ANCHOR_RING"):
        ck(sym not in trace_h_outside,
           "QuorumTrace.h: %s never appears outside its #ifdef QUORUM_TRACE region" % sym)

    # requirement 2: the subscription exists once QUORUM_TRACE IS defined.
    # Source-level half here; the binary-level half (grepping the compiled
    # ELF for the literal topic string -- present with trace ON, absent
    # with trace OFF) was verified directly against arduino-cli's output
    # this session and is reported in the delivered compile results, not
    # reproduced here since this script cannot invoke the ESP32 toolchain.
    subscribeall_body = fns.get("subscribeAll", "").replace(" ", "")
    ck("mqtt.subscribe(T_CMD_TRACE_ANCHOR)" in subscribeall_body,
       "subscribeAll() subscribes T_CMD_TRACE_ANCHOR (requirement 2, source half)")

    # requirement 3: existing command topics/handlers are unchanged -- every
    # ORIGINAL topic build line, subscribe call and handler branch is still
    # present verbatim; the new one is additive only.
    ORIGINAL_TOPIC_LITERALS = [
        "ngr/loco/%s/cmd/auto", "ngr/loco/%s/cmd/direction",
        "ngr/loco/%s/cmd/session_direction", "ngr/loco/%s/cmd/start_mm",
        "ngr/loco/%s/cmd/start_interval", "ngr/loco/%s/cmd/dispatcher_release",
        "ngr/loco/%s/cmd/force_lost", "ngr/loco/%s/cmd/estop",
        "ngr/loco/%s/cmd/throttle",
    ]
    buildtopics_raw = fns_raw.get("buildTopics", "")
    for lit in ORIGINAL_TOPIC_LITERALS:
        ck(lit in buildtopics_raw, "buildTopics() still builds \"%s\" unchanged" % lit)
    ORIGINAL_SUBSCRIBED = ["T_CMD_AUTO", "T_CMD_GO", "T_CMD_STOP", "T_CMD_DIR",
                           "T_CMD_SESSDIR", "T_CMD_STARTMM", "T_CMD_ESTOP",
                           "T_CMD_ESTOP_ALL", "T_CMD_THROTTLE", "T_CMD_STARTINT",
                           "T_CMD_RELEASE", "T_CMD_FORCELOST"]
    for var in ORIGINAL_SUBSCRIBED:
        ck(("mqtt.subscribe(%s)" % var) in subscribeall_body,
           "subscribeAll() still subscribes %s, unchanged" % var)
    handle_body = fns.get("handleCommand", "")
    handle_body_flat = handle_body.replace(" ", "")
    ORIGINAL_HANDLER_VARS = ORIGINAL_SUBSCRIBED + ["T_CMD_ESTOP_ALL"]
    for var in set(ORIGINAL_HANDLER_VARS):
        ck(("topic,%s)" % var) in handle_body_flat,
           "handleCommand() still tests topic against %s, unchanged" % var)

    # requirement 4: anchor commands pass through the existing command
    # queue -- the new branch lives inside handleCommand() (called only
    # from serviceCommands(), which drains cmdQueue), never inside
    # onMqttEnqueue() (which would bypass the queue entirely).
    ck("T_CMD_TRACE_ANCHOR" in handle_body, "the trace-anchor branch is inside handleCommand()")
    enqueue_body = fns.get("onMqttEnqueue", "")
    ck("T_CMD_TRACE_ANCHOR" not in enqueue_body,
       "the trace-anchor branch is NOT in onMqttEnqueue() -- no bypass of cmdQueue")
    ck("handleCommand(" in fns.get("serviceCommands", "").replace(" ", ""),
       "serviceCommands() (the cmdQueue-drain loop) is what calls handleCommand()")

    # Isolate the anchor branch itself for the checks below (requirements
    # 8, 12, 13 need to look at ONLY this branch, not all of handleCommand(),
    # which legitimately writes plenty of control state in its OTHER
    # branches).
    anchor_branch = extract_block(
        handle_body,
        r"else\s*if\s*\(\s*!\s*strcmp\s*\(\s*topic\s*,\s*T_CMD_TRACE_ANCHOR\s*\)\s*\)")
    ck(bool(anchor_branch), "the T_CMD_TRACE_ANCHOR branch was isolated for requirement 8/12/13 checks")

    # requirements 5 + 11: anchor ids are monotonic per session, and a
    # reboot (qtTraceBegin(), called once per boot from setup()) resets
    # both the id counter and the ring, so a new session starts a new
    # anchor sequence rather than continuing the previous boot's.
    submit_body = fns.get("qtSubmitAnchor", "")
    ck("++qtAnchorIdCounter" in submit_body.replace(" ", ""),
       "qtSubmitAnchor() assigns ids by pre-incrementing a single counter (requirement 5)")
    id_writes = re.findall(r"qtAnchorIdCounter\s*(?:=[^=]|\+\+)", src)
    ck(len(id_writes) == 2,
       "qtAnchorIdCounter is written in exactly two places in the whole sketch "
       "(the increment and the per-session reset)",
       "found %d write site(s)" % len(id_writes))
    tracebegin_body = fns.get("qtTraceBegin", "")
    ck("qtAnchors.begin(" in tracebegin_body.replace(" ", ""),
       "qtTraceBegin() resets the anchor ring every boot (requirement 11)")
    ck("qtAnchorIdCounter=0" in tracebegin_body.replace(" ", ""),
       "qtTraceBegin() resets the anchor id counter every boot (requirements 5+11)")

    # requirements 6+7 (static half -- verbatim survival through the ring
    # and wire format is proven for real by test_quorum_trace.cpp and
    # test_qt_decoder.py): qtSubmitAnchor() sources sampleSeq/dir/pwm from
    # the right places, not a re-derivation that could disagree with them.
    ck("a.sampleSeq=sampleSeq" in submit_body.replace(" ", ""),
       "qtSubmitAnchor() stamps sampleSeq from its caller-supplied snapshot -- the caller's "
       "ack (handleCommand()) and the stored record are guaranteed to agree on the same value")
    ck("a.dir=qtEncodeDir(navDir)" in submit_body.replace(" ", ""),
       "qtSubmitAnchor() stamps direction from navDir directly (loop-thread-owned here; "
       "no cross-core mirror needed the way hallTask's qtSampleTick() needs one)")
    ck("a.pwmActual=(uint8_t)actualPwm" in submit_body.replace(" ", "") and
       "a.pwmCommanded=(uint8_t)commandedPwm" in submit_body.replace(" ", ""),
       "qtSubmitAnchor() stamps both PWM values from the existing volatile globals")

    # requirement 8 (static half -- the accept/truncate/reject rule itself
    # is host-tested for real by test_quorum_trace.cpp's
    # testAnchorTextDecisionRule): handleCommand()'s branch defers to
    # qtDecideAnchorText() rather than reimplementing its own truncation.
    ck("qtDecideAnchorText(msg)" in anchor_branch.replace(" ", ""),
       "handleCommand()'s anchor branch defers text accept/truncate/reject to "
       "qtDecideAnchorText() (requirement 8) instead of reimplementing the rule inline")

    # requirement 9: overflow is bounded (QtAnchorRing, proven by
    # test_quorum_trace.cpp), counted, and reported in STATUS.
    ck("st.cumAnchorRingDrops=qtAnchors.cumRingDrops()" in fns.get("qtStatusService", "").replace(" ", ""),
       "qtStatusService() carries the anchor ring's overflow counter into STATUS (requirement 9)")

    # requirement 12: an anchor cannot write any navigation, motor, E-stop,
    # Auto, command-authority or evidence-ring state. Reuses CONTROL_WRITE
    # from the requirement-8 section above, plus a new evidence-ring regex,
    # against qtSubmitAnchor(), qtPublishAnchorAck() and the anchor branch
    # itself (none of which are in QT_FUNCS, since they are reached via
    # direct #ifdef, not the macro layer).
    RING_WRITE = re.compile(r"(?<![.>\w])(evRing|evRingLen|evRingHead)\s*(=[^=]|\+\+|--|\[)")
    for name, body in (("qtSubmitAnchor", submit_body),
                       ("qtPublishAnchorAck", fns.get("qtPublishAnchorAck", "")),
                       ("the T_CMD_TRACE_ANCHOR branch", anchor_branch)):
        bad = CONTROL_WRITE.search(body)
        ck(not bad, "%s writes no motor/navigation/command state (requirement 12)" % name,
           bad.group(0) if bad else "")
        bad_ring = RING_WRITE.search(body)
        ck(not bad_ring, "%s writes no evidence-ring state (requirement 12)" % name,
           bad_ring.group(0) if bad_ring else "")
        ck("digitalWrite" not in body and "ledcWrite" not in body and "pwmWriteCompat" not in body,
           "%s contains no direct motor pin write" % name)
        ck("requestPwm" not in body and "applyDirection" not in body and
           "navDeclare" not in body and "acceptEvent(" not in body and
           "navOnMarker(" not in body and "stationReset" not in body,
           "%s does not call back into any control-path function" % name)

    # requirement 13: changing or losing anchors cannot affect QUORUM
    # behaviour -- no control-path function ever reads anchor state back.
    CONTROL_FUNCS_TO_CHECK = ["detectorSample", "hallTask", "navOnMarker", "acceptEvent",
                              "applyDirection", "navPublishState", "publishQuorumDecision"]
    ANCHOR_TOKENS = ("qtAnchor", "QtAnchor")
    for cf in CONTROL_FUNCS_TO_CHECK:
        body = fns.get(cf, "")
        found = [t for t in ANCHOR_TOKENS if t in body]
        ck(not found, "%s() never reads anchor state back (requirement 13)" % cf,
           "found: %s" % ", ".join(found) if found else "")
    # handleCommand() legitimately MENTIONS T_CMD_TRACE_ANCHOR/qtSubmitAnchor
    # (it dispatches to the branch) -- check its OTHER branches only, by
    # checking the body with the anchor branch itself removed.
    handle_minus_anchor = handle_body.replace(anchor_branch, "") if anchor_branch else handle_body
    found = [t for t in ANCHOR_TOKENS if t in handle_minus_anchor]
    ck(not found, "handleCommand()'s OTHER branches never read anchor state back (requirement 13)",
       "found: %s" % ", ".join(found) if found else "")

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
