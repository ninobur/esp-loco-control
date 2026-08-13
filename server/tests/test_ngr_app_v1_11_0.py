#!/usr/bin/env python3
"""Exercise ngr_app_v1_11_0 without a broker or locomotives: feed
on_mqtt_message() directly and drive the routes through Flask's test client.

    python3 server/tests/test_ngr_app_v1_11_0.py

The MQTT thread will print "Connection refused" on the way past; that is the
app trying to reach a broker that is not there, and is not a test failure.
Exit status is 0 only if every assertion passes."""
import sys, os, json
# repo-relative: server/tests/ -> server/
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import ngr_app_v1_11_0 as A

FAILS = []
def ok(cond, label, extra=""):
    print(("  PASS  " if cond else "  FAIL  ") + label + (("   " + str(extra)) if extra and not cond else ""))
    if not cond:
        FAILS.append(label)

class Msg:
    def __init__(self, topic, payload, retain=False):
        self.topic = topic
        self.payload = payload.encode() if isinstance(payload, str) else payload
        self.retain = retain

def feed(topic, payload, retain=False):
    A.on_mqtt_message(None, None, Msg(topic, payload, retain))

# Published commands land here instead of a broker.
PUBLISHED = []
A.pub = lambda topic, value: PUBLISHED.append((topic, str(value)))

c = A.app.test_client()

print("\n== console with nothing switched on ==")
r = c.get("/console")
ok(r.status_code == 200, "console renders with nothing heard", r.status_code)
j = c.get("/dispatcher/state").get_json()
ok([l["name"] for l in j["locos"]] == ["Otto", "Toby", "Hans"],
   "every NAMED locomotive has a column before anything speaks",
   [l["name"] for l in j["locos"]])
o = j["locos"][0]
ok(o["heard"] is False and o["mode"] == "NONE" and o["quorum"] == "UNKNOWN",
   "and it is credited with nothing: not heard, no authority, no quorum",
   (o["heard"], o["mode"], o["quorum"]))
ok(o["mm"] == "--" and o["pwm"] == "--" and o["agree_pct"] is None,
   "no figures are invented for a locomotive that has never spoken")
ok(dict(A.loco_state) == {},
   "being LISTED is not being HEARD — loco_state is still empty", list(A.loco_state))
r = c.get("/")
ok(r.status_code == 302 and "/console" in r.headers["Location"], "root redirects to console")

print("\n== END AO reaches a column that has not been heard ==")
PUBLISHED.clear()
c.post("/dispatcher/endcto")
ok(sorted(t.split("/")[2] for t, v in PUBLISHED if t.endswith("/cmd/dispatcher_release"))
   == ["2095111", "9950011", "9950012"],
   "release goes to every column on screen, heard or not", PUBLISHED)
PUBLISHED.clear()

print("\n== the discovery gate ==")
feed("ngr/loco/9950011/state/mm", "42", retain=True)
ok("9950011" not in A.loco_state, "a RETAINED message does not create a locomotive")
feed("ngr/loco/9950011/cmd/throttle", "120", retain=False)
ok("9950011" not in A.loco_state, "a cmd/ echo does not create a locomotive")
feed("ngr/loco/9950011/state/online", "1", retain=True)
ok("9950011" not in A.loco_state, "a retained online does not create a locomotive")

feed("ngr/loco/9950011/alert", json.dumps({"nav": "NORMAL", "dead_reckoned_mm": 82,
                                           "moving": 1, "pwm": 146, "auto": 0,
                                           "agree": 4015, "disagree": 30}))
ok("9950011" in A.loco_state, "a LIVE message creates a locomotive")
ok(A.loco_state["9950011"]["mm"] == "082", "position landed", A.loco_state["9950011"]["mm"])

print("\n== retained still updates an existing locomotive (P8 seed) ==")
feed("ngr/loco/9950011/online", "1", retain=True)
feed("ngr/loco/9950011/state/auto", "1", retain=True)
ok(A.loco_state["9950011"]["enlisted"] == "1", "P8 seed promoted once online=1 proved alive",
   A.loco_state["9950011"]["enlisted"])

print("\n== the retained backlog arrives BEFORE anything live ==")
# The real connect order, and the bug found on the first side-by-side run
# against the live broker: the broker replays retained topics the instant we
# subscribe, so loco_state is still empty when the P8 seeds turn up. Dropping
# them left an ENLISTED locomotive reading "never proven this session".
for k in list(A.loco_state):
    A.loco_state.pop(k); A.loco_rx.pop(k, None); A.loco_epoch.pop(k, None)
    A.loco_seed.pop(k, None); A.loco_seed_online.pop(k, None); A.loco_log.pop(k, None)
A.pending_seed.clear(); A.pending_seed_online.clear()

feed("ngr/loco/9950011/online", "1", retain=True)           # backlog...
feed("ngr/loco/9950011/state/auto", "1", retain=True)
feed("ngr/loco/9950011/state/estop", "0", retain=True)
ok("9950011" not in A.loco_state, "the backlog alone still creates nothing")
ok(A.pending_seed.get("9950011", {}).get("state/auto") == "1",
   "the seed is held aside rather than thrown away")
feed("ngr/loco/9950011/alert", json.dumps({"nav": "NORMAL", "dead_reckoned_mm": 29,
                                           "moving": 0, "auto": 0}))   # ...then live
ok("9950011" in A.loco_state, "the live message is what creates the locomotive")
ok(A.loco_state["9950011"]["enlisted"] == "1",
   "and the held P8 seed is promoted, so ENLISTED survives a restart",
   A.loco_state["9950011"]["enlisted"])
ok(A._mode_of(A.loco_state["9950011"]) == "ENL",
   "the console reads ENLISTED, not NONE", A._mode_of(A.loco_state["9950011"]))
ok("9950011" not in A.pending_seed, "the held seed is consumed, not left to leak")

print("\n== a last will in the backlog voids the seeds ==")
A.loco_state.pop("9950011"); A.loco_rx.pop("9950011", None)
A.pending_seed.clear(); A.pending_seed_online.clear()
feed("ngr/loco/9950012/state/auto", "1", retain=True)
feed("ngr/loco/9950012/online", "0", retain=True)      # last will fired
feed("ngr/loco/9950012/alert", json.dumps({"nav": "NORMAL", "dead_reckoned_mm": 7}))
ok(A.loco_state["9950012"]["enlisted"] == "--",
   "a locomotive whose last will fired is not credited with authority",
   A.loco_state["9950012"]["enlisted"])

print("\n== the held buffer is bounded ==")
A.pending_seed.clear(); A.pending_seed_online.clear()
for i in range(A.PENDING_SEED_MAX + 40):
    feed("ngr/loco/JUNK%04d/state/auto" % i, "1", retain=True)
ok(len(A.pending_seed) <= A.PENDING_SEED_MAX,
   "a flood of unknown ids cannot grow the buffer without limit", len(A.pending_seed))
ok(not any(k.startswith("JUNK") for k in A.loco_state),
   "and none of them became a column")
ok(not any(str(l["id"]).startswith("JUNK") for l in c.get("/dispatcher/state").get_json()["locos"]),
   "nor reached the console")
A.pending_seed.clear(); A.pending_seed_online.clear()
feed("ngr/loco/9950011/alert", json.dumps({"nav": "NORMAL", "dead_reckoned_mm": 82,
                                           "moving": 1, "pwm": 146, "auto": 0,
                                           "agree": 4015, "disagree": 30}))

print("\n== an unnamed locomotive is first-class ==")
feed("ngr/loco/7777777/alert", json.dumps({"nav": "NO_QUORUM", "moving": 0}))
ok("7777777" in A.loco_state, "unknown id discovered without being named anywhere")
ok(A.loco_name("7777777") == "7777777", "unnamed locomotive is labelled with its id")

print("\n== stable order ==")
feed("ngr/loco/2095111/alert", json.dumps({"nav": "NORMAL", "dead_reckoned_mm": 5}))
feed("ngr/loco/9950012/alert", json.dumps({"nav": "NORMAL", "dead_reckoned_mm": 9}))
order = A.discovered_order()
ok(order == ["9950011", "9950012", "2095111", "7777777"],
   "known names in listed order, strangers after", order)
ok(A.console_order() == order,
   "console order matches once everything named has been heard", A.console_order())
# arrival order was 9950011, 7777777, 2095111, 9950012 — deliberately not this
ok(order != list(A.loco_state.keys()), "order is NOT arrival order")

print("\n== authority display ==")
st = A.loco_state["9950011"]
st["auto"], st["ce"], st["enlisted"] = "0", "0", "1"
ok(A._mode_of(st) == "ENL", "enrolled but not running reads ENLISTED", A._mode_of(st))
st["auto"] = "1"
ok(A._mode_of(st) == "RUN", "autoRunning reads RUNNING", A._mode_of(st))
st["ce"] = "1"
ok(A._mode_of(st) == "CE", "circuit express reads CE", A._mode_of(st))
st["auto"], st["ce"], st["enlisted"] = "0", "0", "0"
ok(A._mode_of(st) == "MAN", "neither reads MANUAL", A._mode_of(st))
st["enlisted"] = "--"
ok(A._mode_of(st) == "NONE", "never proven this session reads NONE", A._mode_of(st))
st["enlisted"] = "0"

print("\n== quorum ==")
for nav, heard, want in [("NORMAL", True, "QUORUM"), ("TRACKING", True, "QUORUM"),
                         ("EVALUATING", True, "EVALUATING"), ("NO_QUORUM", True, "NO_QUORUM"),
                         ("LOST", True, "NO_QUORUM"), ("UNSET", True, "UNSET"),
                         ("NORMAL", False, "UNKNOWN")]:
    st["nav"] = nav
    got = A._quorum_of(st, heard)
    ok(got == want, "nav %s heard=%s -> %s" % (nav, heard, want), got)

print("\n== rolling agreement is not the session total ==")
st["agree_n"], st["disagree_n"] = 4015, 30
st["verdicts"] = [[65,0],[66,0],[67,0],[76,0],[77,0],[78,0],[79,1],[80,1],[81,0],[82,0]]
ok(A._rolling_agree(st) == 20, "last ten read 20% while the session reads 99%",
   A._rolling_agree(st))
st["verdicts"] = []
ok(A._rolling_agree(st) is None, "no verdicts yields no percentage, not zero")

print("\n== routes ==")
r = c.get("/loco/otto")
ok(r.status_code == 200, "loco page by name", r.status_code)
ok(b"Otto" in r.data and b"STARTUP" in r.data and b"SET LOCATION" in r.data, "page carries the name and the STARTUP block")
ok(b"CAL RECORDING" not in r.data, "CAL recording is gone")
ok(b"LAST 10 VERDICTS" in r.data, "agreement is labelled as a rolling window")
ok(r.data.count(b"dir-btn") > 0 and b'id="dir-btn-1"' not in r.data,
   "direction is FOR/REV — no neutral BUTTON")
ok(b'id="neutral-chip"' in r.data, "neutral is still displayable when reported")
r = c.get("/loco/7777777")
ok(r.status_code == 200, "loco page by bare id", r.status_code)
r = c.get("/loco/nosuchloco")
ok(r.status_code == 404, "unknown name 404s", r.status_code)
r = c.get("/otto")
ok(r.status_code == 302 and r.headers["Location"].endswith("/loco/otto"),
   "legacy /otto redirects", r.headers.get("Location"))
r = c.get("/loco/otto/state")
ok(r.status_code == 200 and "ages" in r.get_json(), "state payload has ages")

print("\n== a named locomotive never heard ==")
saved = A.loco_state.pop("2095111")
r = c.get("/loco/hans/state")
j = r.get_json()
ok(r.status_code == 200, "unheard-but-named answers rather than erroring", r.status_code)
ok(j["direction"] == "--" and j["enlisted"] == "--",
   "blank-until-proven: nothing is fabricated", (j["direction"], j["enlisted"]))
ok(all(v is None for v in j["ages"].values()), "no ages for a locomotive never heard")
A.loco_state["2095111"] = saved

print("\n== dispatcher fan-out walks the discovered set ==")
PUBLISHED.clear()
c.post("/dispatcher/endcto")
rel = sorted(t for t, v in PUBLISHED if t.endswith("/cmd/dispatcher_release"))
stp = sorted(t for t, v in PUBLISHED if "/dispatcher/cmd/stop/" in t)
ok(len(rel) == 4, "END AO releases every discovered locomotive", rel)
ok(len(stp) == 4, "END AO stops every discovered locomotive", stp)
ok(any("2095111" in t for t in rel), "Hans is RELEASED, not just stopped (the v1.10.11 gap)")
ok(sorted(t.split("/")[2] for t in rel) == sorted(t.split("/")[-1] for t in stp),
   "released set and stopped set are identical")

PUBLISHED.clear()
c.post("/dispatcher/cmd/go")
ok(len([t for t, v in PUBLISHED if "/dispatcher/cmd/go/" in t]) == 4,
   "bare go fans out per locomotive (P2)", PUBLISHED)
PUBLISHED.clear()
c.post("/dispatcher/cmd/estop")
ok(PUBLISHED == [("ngr/dispatcher/cmd/estop", "1")], "E-STOP ALL broadcasts (P12)", PUBLISHED)
PUBLISHED.clear()
c.post("/dispatcher/cmd/estopclear/9950011")
ok(PUBLISHED == [("ngr/loco/9950011/cmd/estop", "0")],
   "E-STOP clear is per-locomotive (P12)", PUBLISHED)
PUBLISHED.clear()
c.post("/dispatcher/cmd/go/8888888")
ok(PUBLISHED == [], "a command for an undiscovered locomotive publishes nothing", PUBLISHED)

print("\n== E-STOP is never gated by AUTO ==")
A.loco_state["9950011"]["auto"] = "1"
PUBLISHED.clear()
r = c.post("/loco/otto/cmd/estop/1")
ok(r.status_code == 204 and PUBLISHED == [("ngr/loco/9950011/cmd/estop", "1")],
   "E-STOP publishes while enlisted", (r.status_code, PUBLISHED))
PUBLISHED.clear()
r = c.post("/loco/otto/cmd/throttle/120")
ok(r.status_code == 423 and PUBLISHED == [],
   "throttle is refused while enlisted", (r.status_code, PUBLISHED))
A.loco_state["9950011"]["auto"] = "0"
PUBLISHED.clear()
r = c.post("/loco/otto/cmd/throttle/120")
ok(r.status_code == 204 and PUBLISHED == [("ngr/loco/9950011/cmd/throttle", "120")],
   "throttle publishes in manual", (r.status_code, PUBLISHED))

print("\n== console reflects discovery ==")
j = c.get("/dispatcher/state").get_json()
ok(len(j["locos"]) == 4, "every discovered locomotive is offered to the console")
ok([l["id"] for l in j["locos"]] == A.console_order(), "console order matches the stable order")
ok(all(k in j["locos"][0] for k in ("mode", "quorum", "mm", "pkph", "pwm", "voltage", "agree_pct")),
   "console payload carries the new figures", list(j["locos"][0]))

print("\n== a locomotive that goes quiet keeps its column ==")
n_before = len(A.loco_state)
A.loco_rx["7777777"].clear()          # nothing heard for a long time
j = c.get("/dispatcher/state").get_json()
ok(len(A.loco_state) == n_before, "nothing is ever pruned from the roster")
q = [l for l in j["locos"] if l["id"] == "7777777"][0]
ok(q["heard"] is False, "it reports not-heard")
ok(q["quorum"] == "UNKNOWN", "and its quorum is UNKNOWN, not its last value", q["quorum"])

print("\n== the log names locomotives ==")
r = c.get("/dispatcher/log")
entries = r.get_json()
ok(isinstance(entries, list) and entries and "name" in entries[0],
   "dispatcher log entries carry a display name")

print("\n" + ("ALL PASS" if not FAILS else "FAILURES: " + "; ".join(FAILS)))
sys.exit(1 if FAILS else 0)
