// Host tests for RvmStations.h: rainbow-carpet station stops and the two-train
// bubble. A crude kinematic model drives the pure logic the way the sketch does
// (one StationDriver per locomotive, PWM ramps at the sketch's rates, peers
// exchanging PeerPacket every 250 ms). It tests the logic, not the railway.
//   c++ -std=c++17 -O2 -Wall -Wextra -Werror -I<sketch dir> test_bubble.cpp -o t && ./t
#include <cmath>
#include <cstdio>
#include <vector>
#include "RvmStations.h"

using namespace rvm_ops;
static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++failures; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// Kinematics: speed (MM/s) = applied PWM / 90, nothing below PWM 20. The
// committed MM is the last MM passed, reported RVM_DELAY_MS after passing it
// (NAVI_RVM reports ~1 s after an ensemble; 1.5 s here is pessimistic).
static constexpr uint32_t DT = 10, RVM_DELAY_MS = 1500;

struct Loco {
  uint32_t id = 0; uint8_t start = 0; int8_t dir = 1;
  double x = 0.3;                 // MM travelled from `start` (sensor just past it, on baseline)
  int applied = 0, target = 0; uint16_t upMs = 150, downMs = 31; uint32_t lastStep = 0;
  bool autoOn = true, radioOn = true, tracking = true;
  StationDriver drv; DriveOutput last;
  std::vector<std::pair<uint32_t, uint8_t>> passes; size_t nextPass = 0; uint8_t committed = 0;
  // stops (PWM 0 for >= 4 s): physical MM floor at the stop
  bool stopped = false; uint32_t zeroSince = 0; std::vector<uint8_t> stops; std::vector<double> stopX;
  std::vector<uint8_t> brakeAt; StopExecution prevEx = StopExecution::Geographic;   // committed MM where braking began
  uint8_t mmAt(double u) const { return routeMod(int32_t(start) + dir * int32_t(std::floor(u))); }
  double physical() const { double p = start + dir * x; p = std::fmod(p, double(ROUTE_N)); return p < 0 ? p + ROUTE_N : p; }
};
static void init(Loco& l, uint32_t id, uint8_t start, int8_t dir) { l.id = id; l.start = start; l.dir = dir; l.committed = start; }

static PeerPacket packetOf(const Loco& l) {
  PeerPacket p; p.locoId = l.id; p.reliable = l.tracking; p.dir = l.dir; p.lastMm = l.committed;
  p.station = l.last.station; p.execution = uint8_t(l.last.cmd.execution); p.autoRunning = l.autoOn; p.bubble = uint8_t(l.last.bubble); p.role = uint8_t(l.last.role);
  return p;
}

static void step(Loco& l, const PeerView& peer, uint32_t t) {
  const double v = l.applied >= 20 ? l.applied / 90.0 : 0.0;
  const int64_t f0 = int64_t(std::floor(l.x));
  l.x += v * DT / 1000.0;
  for (int64_t k = f0 + 1; k <= int64_t(std::floor(l.x)); ++k) l.passes.push_back({t, l.mmAt(double(k))});
  while (l.nextPass < l.passes.size() && t - l.passes[l.nextPass].first >= RVM_DELAY_MS) l.committed = l.passes[l.nextPass++].second;
  DriveInput in; in.reliable = true; in.pos = l.committed; in.dir = l.dir; in.cruise = 90;
  in.appliedPwm = uint8_t(l.applied); in.commandedPwm = uint8_t(l.target); in.nowMs = t; in.enabled = l.autoOn; in.peer = peer;
  l.last = l.drv.step(in);
  if (l.last.cmd.execution == StopExecution::Braking && l.prevEx != StopExecution::Braking) l.brakeAt.push_back(l.committed);
  l.prevEx = l.last.cmd.execution;
  const auto& c = l.last.cmd;
  if (l.target != c.pwm) { l.downMs = (c.execution == StopExecution::Braking && !l.last.urgent) ? 300 : 31; l.upMs = 150; l.target = c.pwm; }
  if (l.applied != l.target) {
    const bool down = l.target < l.applied; const uint16_t s = down ? l.downMs : l.upMs;
    if (t - l.lastStep >= s) { l.lastStep = t; l.applied += down ? -1 : 1; }
  }
  if (l.applied == 0) { if (!l.stopped) { l.stopped = true; l.zeroSince = t; } }
  else if (l.stopped) { l.stopped = false; if (t - l.zeroSince >= 4000) { l.stops.push_back(l.mmAt(l.x)); l.stopX.push_back(l.x); } }
}
// Gap from f forward to ld (MM), along f's direction.
static double gap(const Loco& f, const Loco& ld) {
  double d = (ld.physical() - f.physical()) * f.dir; d = std::fmod(d, double(ROUTE_N)); return d < 0 ? d + ROUTE_N : d;
}
struct Radio { PeerView view; uint32_t lastTx = 0; };
static void exchange(Radio& rxAtA, const Loco& b, uint32_t t) {
  if (b.radioOn && t - rxAtA.lastTx >= 250) { rxAtA.view.p = packetOf(b); rxAtA.view.everSeen = true; rxAtA.lastTx = t; }
  rxAtA.view.ageMs = rxAtA.view.everSeen ? t - rxAtA.lastTx : 0;
}
static bool isStation(uint8_t m) { for (auto& s : STATIONS) if (s.mm == m) return true; return false; }

int main() {
  // ---- T1: carpet PWM by whole-MM distance to the stopping MM.
  {
    const int want[12] = {0, 43, 50, 50, 50, 50, 50, 50, 60, 70, 80, 90};
    for (int d = 0; d <= 11; ++d) {
      const auto g = mmGeography(routeMod(63 - d), 63, 1), h = mmGeography(routeMod(63 + d), 63, -1);
      CHECK(g.applicable() && g.pwm == want[d], "CW d=%d pwm=%u want %d", d, g.pwm, want[d]);
      CHECK(h.applicable() && h.pwm == want[d], "CCW d=%d pwm=%u want %d", d, h.pwm, want[d]);
    }
    CHECK(!mmGeography(51, 63, 1).applicable(), "12 MM out must be outside the carpet");
    CHECK(!mmGeography(64, 63, 1).applicable(), "past the station must be outside");
  }
  // ---- T2: solo, both directions, two laps: stops only at the stations, in route order.
  for (int8_t dir : {int8_t(1), int8_t(-1)}) {
    Loco a; init(a, 1, dir > 0 ? 20 : 150, dir); PeerView none;
    for (uint32_t t = 0; t < 900000; t += DT) step(a, none, t);
    printf("T2 dir %+d: %zu stops (physical MM after the model's coast):", dir, a.stops.size()); for (auto m : a.stops) printf(" %u", m);
    printf("; braking began at committed"); for (auto m : a.brakeAt) printf(" %u", m); printf("\n");
    CHECK(a.stops.size() >= 8, "dir %d: expected two laps of 4 stations, got %zu", dir, a.stops.size());
    CHECK(a.brakeAt.size() == a.stops.size() || a.brakeAt.size() == a.stops.size() + 1, "dir %d: %zu brakings for %zu stops", dir, a.brakeAt.size(), a.stops.size());
    for (auto m : a.brakeAt) CHECK(isStation(m), "dir %d: braking began at MM%u, not a station", dir, m);
    for (size_t i = 1; i < a.brakeAt.size(); ++i) CHECK(a.brakeAt[i] != a.brakeAt[i - 1], "dir %d: station MM%u twice in a row", dir, a.brakeAt[i]);
  }
  // ---- T3: bubble. Leader A ahead of follower B, both CW. B must hold 5 MM short of each
  // platform A occupies, never close on A, then make its own platform stop.
  for (int8_t dir : {int8_t(1), int8_t(-1)}) {
    Loco a, b; init(a, 1, dir > 0 ? 50 : 76, dir); init(b, 2, dir > 0 ? 40 : 86, dir);
    Radio atA, atB; double minGap = 1e9; int holds = 0; bool wasHolding = false;
    std::vector<uint8_t> bHoldMm;
    for (uint32_t t = 0; t < 900000; t += DT) {
      exchange(atA, b, t); exchange(atB, a, t);
      step(a, atA.view, t); step(b, atB.view, t);
      const double g = gap(b, a);
      if (g < minGap) { minGap = g; if (g < 0.0) printf("  t=%.1fs gap %.2f | A x=%.2f MM%u pwm %d %s st%u | B x=%.2f MM%u pwm %d %s kind %s tgt %u bubble %u occ %s\n",
          t / 1000.0, g, a.physical(), a.committed, a.applied, executionName(a.last.cmd.execution), a.last.station,
          b.physical(), b.committed, b.applied, executionName(b.last.cmd.execution), targetKindName(b.last.kind), b.last.targetMm, unsigned(b.last.bubble), occupancyName(b.last.occupancy)); }
      const bool h = b.last.bubble == BubbleState::Holding;
      if (h && !wasHolding) { ++holds; bHoldMm.push_back(b.last.targetMm); }
      wasHolding = h;
    }
    printf("T3 dir %+d: A stops %zu, B stops %zu, B holds %d at", dir, a.stops.size(), b.stops.size(), holds);
    for (auto m : bHoldMm) printf(" %u", m); printf("; min gap %.2f MM\n", minGap);
    CHECK(minGap >= 2.0, "dir %d: follower closed to %.2f MM of the leader", dir, minGap);
    CHECK(holds >= 1, "dir %d: the follower never held", dir);
    for (auto m : bHoldMm) {
      bool ok = false; for (auto& s : STATIONS) if (m == before(s.mm, dir, BUBBLE_HOLD_MM)) ok = true;
      CHECK(ok, "dir %d: hold target MM%u is not 5 MM before a station", dir, m);
    }
    CHECK(a.stops.size() >= 6 && b.stops.size() >= 6, "dir %d: trains stopped serving stations", dir);
  }
  // ---- T4: peer radio silent while the follower approaches: it holds and does not advance.
  {
    Loco a, b; init(a, 1, 50, 1); init(b, 2, 40, 1); Radio atA, atB;
    uint32_t t = 0;
    for (; t < 20000; t += DT) { exchange(atA, b, t); exchange(atB, a, t); step(a, atA.view, t); step(b, atB.view, t); }
    a.radioOn = false;
    for (; t < 400000; t += DT) { exchange(atA, b, t); exchange(atB, a, t); step(a, atA.view, t); step(b, atB.view, t); }
    printf("T4: B committed MM%u, kind %s, bubble %u, link %s, role %s\n", b.committed, targetKindName(b.last.kind), unsigned(b.last.bubble), linkName(b.last.link), roleName(b.last.confirmedRole));
    CHECK(b.last.link == Link::Lost && b.last.occupancy == Occupancy::Lost, "silent paired peer must read LOST");
    CHECK(b.last.confirmedRole == Role::Follower, "B was the follower");
    CHECK(b.last.bubble == BubbleState::Holding && b.applied == 0 && b.last.kind == TargetKind::LostStop, "follower must make a carpet stop when the leader is silent");
    // radio back: the follower resumes and serves the station
    a.radioOn = true; const size_t before = b.stops.size();
    for (; t < 700000; t += DT) { exchange(atA, b, t); exchange(atB, a, t); step(a, atA.view, t); step(b, atB.view, t); }
    CHECK(b.stops.size() > before && b.last.link == Link::Known, "follower must resume when the peer is heard again");
    printf("T4b: after radio restored B stops %zu -> %zu\n", before, b.stops.size());
  }
  // ---- T5: occupancy geometry (KNOWN link) and role derivation.
  {
    PeerPacket p; p.reliable = 1; p.dir = 1; p.lastMm = 55; p.station = 1; p.execution = uint8_t(StopExecution::Geographic);
    CHECK(stationOccupancy(p, 1, 60, 1) == Occupancy::Free, "peer behind (MM55) while we are at MM60 for Grillers");
    p.lastMm = 61; CHECK(stationOccupancy(p, 1, 57, 1) == Occupancy::Occupied, "peer at MM61, between hold 58 and 63");
    p.lastMm = 64; p.execution = uint8_t(StopExecution::Released); CHECK(stationOccupancy(p, 1, 57, 1) == Occupancy::Free, "peer released and past");
    p.lastMm = 63; p.execution = uint8_t(StopExecution::Dwell); CHECK(stationOccupancy(p, 1, 52, 1) == Occupancy::Occupied, "peer dwelling at the platform");
    p.execution = uint8_t(StopExecution::Released); CHECK(stationOccupancy(p, 1, 52, 1) == Occupancy::Free, "leader leaves the platform");
    CHECK(deriveRole(40, 50, 1) == Role::Follower && deriveRole(50, 40, 1) == Role::Leader, "CW roles");
    CHECK(deriveRole(50, 40, -1) == Role::Follower && deriveRole(40, 50, -1) == Role::Leader, "CCW roles");
    CHECK(deriveRole(40, 40, 1) == Role::None, "same MM: no role");
    for (int a = 0; a < 171; a += 7) for (int b = 0; b < 171; b += 5) if (a != b)
      CHECK(deriveRole(a, b, 1) == opposite(deriveRole(b, a, 1)), "roles complementary %d %d", a, b);
  }
  // ---- T6: the leader stops dead between stations (CONTRADICTION: reliable=0). The follower stops at once.
  {
    Loco a, b; init(a, 1, 50, 1); init(b, 2, 40, 1); Radio atA, atB; double minGap = 1e9; uint32_t t = 0;
    for (; t < 25000; t += DT) { exchange(atA, b, t); exchange(atB, a, t); step(a, atA.view, t); step(b, atB.view, t); }
    printf("T6: at failure leader %.1f pwm %d, follower %.1f pwm %d\n", a.physical(), a.applied, b.physical(), b.applied);
    a.autoOn = false; a.applied = a.target = 0;            // leader dead on the main line at ~MM53
    for (; t < 200000; t += DT) {
      exchange(atA, b, t); exchange(atB, a, t);
      a.tracking = false;                                  // leader reports CONTRADICTION (reliable 0)
      step(b, atB.view, t); minGap = std::min(minGap, gap(b, a));
    }
    printf("T6: leader dead at %.1f, follower at %.1f, min gap %.2f MM\n", a.physical(), b.physical(), minGap);
    CHECK(minGap > 1.0 && b.applied == 0 && b.last.kind == TargetKind::LostStop, "follower must carpet-stop short of a leader that lost tracking");
  }
  // ---- T7: silent FOLLOWER: the leader goes on to its next station, stops there and stays.
  {
    Loco a, b; init(a, 1, 50, 1); init(b, 2, 40, 1); Radio atA, atB; uint32_t t = 0;
    for (; t < 3000; t += DT) { exchange(atA, b, t); exchange(atB, a, t); step(a, atA.view, t); step(b, atB.view, t); }
    CHECK(a.last.link == Link::Known && a.last.confirmedRole == Role::Leader, "A must be the confirmed leader");
    b.radioOn = false; b.autoOn = false; b.applied = b.target = 0;
    for (; t < 300000; t += DT) { exchange(atA, b, t); step(a, atA.view, t); }
    printf("T7: leader A committed MM%u, stops %zu, kind %s, platform_held %u, pwm %d\n", a.committed, a.stops.size(),
           targetKindName(a.last.kind), unsigned(a.last.platformHeld), a.applied);
    CHECK(a.applied == 0 && a.last.platformHeld && a.last.kind == TargetKind::Platform && isStation(a.last.targetMm),
          "silent follower: leader must stop at its next platform and stay");
    CHECK(a.stops.size() == 0, "leader must not leave that platform (no completed stop)");
  }
  // ---- T8: running singly. The other loco is powered on the radio but never TRACKING: no pairing.
  {
    Loco a, b; init(a, 1, 20, 1); init(b, 2, 100, 1); b.tracking = false; b.autoOn = false; Radio atA;
    for (uint32_t t = 0; t < 400000; t += DT) { exchange(atA, b, t); step(a, atA.view, t); }
    printf("T8: solo with an undeclared peer on the radio: link %s, stops %zu\n", linkName(a.last.link), a.stops.size());
    CHECK(a.last.link == Link::Solo && !a.last.paired && a.stops.size() >= 4, "an unpaired loco must run singly");
  }
  // ---- T9: pairing ends on a new declaration (reset); a never-heard-again peer then means solo.
  {
    Loco a, b; init(a, 1, 50, 1); init(b, 2, 40, 1); Radio atA, atB; uint32_t t = 0;
    for (; t < 3000; t += DT) { exchange(atA, b, t); exchange(atB, a, t); step(a, atA.view, t); step(b, atB.view, t); }
    CHECK(a.last.paired, "paired");
    b.radioOn = false; for (; t < 8000; t += DT) { exchange(atA, b, t); step(a, atA.view, t); }
    CHECK(a.last.link == Link::Lost, "lost while paired");
    a.drv.reset();                                         // operator declares Otto again
    for (; t < 300000; t += DT) { exchange(atA, b, t); step(a, atA.view, t); }
    printf("T9: after redeclaration: link %s, stops %zu\n", linkName(a.last.link), a.stops.size());
    CHECK(a.last.link == Link::Solo && a.stops.size() >= 2, "after redeclaration the loco runs singly");
  }
  // ---- T10: role echo mismatch (both claim leader) = LOST.
  {
    StationDriver d; DriveInput in; in.reliable = true; in.pos = 50; in.dir = 1; in.enabled = true; in.nowMs = 1000;
    in.peer.everSeen = true; in.peer.ageMs = 100; in.peer.p.reliable = 1; in.peer.p.dir = 1; in.peer.p.lastMm = 40;
    in.peer.p.role = uint8_t(Role::Follower);
    auto o = d.step(in); CHECK(o.link == Link::Known && o.role == Role::Leader, "confirmed leader");
    in.peer.p.role = uint8_t(Role::Leader); in.nowMs = 1100;
    o = d.step(in); CHECK(o.link == Link::Lost, "both claiming leader must be LOST");
  }
  printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
