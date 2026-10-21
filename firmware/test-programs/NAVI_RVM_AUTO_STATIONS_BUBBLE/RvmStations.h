#pragma once
// ---------------------------------------------------------------------------
// RvmStations — AUTO station stops on the rainbow carpet, and the two-train
// BUBBLE. Pure logic: no hardware, no clocks of its own, host-testable
// (tests/navi_rvm_bubble/test_bubble.cpp).
//
// POSITION. NAVI_RVM's committed MM (Navigator::lastMm: the MM whose ensemble
// was most recently recognized) in whole MM. There is no IR and no fine offset
// (operator, 2026-10-20: "IR signal processing is not reliable"). This is the
// EWO_0_6 "OPERATOR_MM" path of stopView(): position = lastMm * kStopMm.
//
// RAINBOW CARPET (U10, operator 2026-10-14). stopGeography() and the
// NaviStopOverlay executor below are copied from NAVI_EWO_0_6
// NaviStopOverlay.h with only the NaviIntegratedCore dependency removed: the
// carpet is laid backward from the stopping tile over the preceding eleven,
// PWM 90 -> 60..90 (Approach, 8-11 MM out) -> 50 (2-7) -> 43 (1) -> 0 (at the
// stopping MM: final braking), then a 5 s zero-PWM dwell, then release to
// cruise. The stopping MM is the station's centre magnet.
//
// STATIONS (EWO_0_6, operator 2026-10-15: stop at the centres):
//   PATIO 15, GRILLERS 63, ARCHES 108, BAMBOO 157.
//
// BUBBLE (operator, 2026-10-20). If the leader is in the station, the follower
// performs the same stop, just BUBBLE_HOLD_MM (5) mile markers earlier, and
// waits there. When the leader leaves the platform, the follower advances to
// the platform and makes its normal station stop.
//   "Leader in the station" = the peer is fresh, tracking, travelling the same
//   direction, and either reports this station with final braking or dwell, or
//   its committed MM lies between our holding point (exclusive) and the
//   station MM (inclusive) and it has not released this station.
//   "Leader leaves the platform" = that is no longer true (its dwell released
//   it, or it is past the station). A follower already stopped at the holding
//   point then waits a random 2-5 s (operator 2026-10-20) before moving up;
//   one still approaching the holding point switches at once.
//   FOLLOW THE LEADER (operator 2026-10-20): beyond stations, the same carpet
//   anchored 5 MM behind the leader's committed MM caps the follower anywhere.
//   Pairing, dynamic roles, the role echo and the silence rule: see "peer"
//   below. A locomotive that has never been paired runs singly.
//   The decision is taken from 16 MM out (11 + 5), so the follower runs the
//   whole carpet, shifted. Once stopped short it stays at zero (latched) until
//   the platform is free, even if it rolled past its holding MM while braking.
//   If the station becomes occupied when we are already inside the holding
//   distance and not yet braking for the platform, we stop where we are
//   (STOP_NOW) and wait the same way.
// No other spacing exists: the bubble acts at stations only (operator choice,
// 2026-10-20). Leader departure never depends on the follower.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"

namespace rvm_ops {

using navi_one::ROUTE_N;
using navi_one::routeMod;

static constexpr uint8_t BUBBLE_HOLD_MM = 5;       // operator 2026-10-20: where the follower stands, MM before the station
// Operator 2026-10-20: the leader (or a solo train) begins braking 10 MM
// before the station centre (the carpet's first reduction, 90 -> 80, is at
// 10 MM); the follower begins braking 15 MM before it, i.e. the same carpet
// ending BUBBLE_HOLD_MM earlier. No extra lead.
static constexpr uint8_t HOLD_BRAKE_LEAD_MM = 0;
static constexpr uint8_t HOLD_TARGET_MM = BUBBLE_HOLD_MM + HOLD_BRAKE_LEAD_MM;   // carpet stopping MM for the hold
static constexpr uint32_t PEER_STALE_MS = 2000;     // 8 missed 4 Hz packets
static constexpr uint32_t STOP_DWELL_MS = 5000;     // EWO_0_6 kStopDwellMs
// Operator 2026-10-20: a follower stopped behind the leader (at a station's
// holding point or anywhere on the line) moves up a random 2-5 s after the
// leader starts; a convoy leader starts a random 0-3 s after the follower
// stands at PWM 0.
static constexpr uint32_t FOLLOWER_START_DELAY_MIN_MS = 2000, FOLLOWER_START_DELAY_MAX_MS = 5000;
static constexpr uint32_t LEADER_START_DELAY_MIN_MS = 0, LEADER_START_DELAY_MAX_MS = 3000;

// ----------------------------------------------------- rainbow carpet (EWO) --
static constexpr int32_t kStopMm = 1000000;
static constexpr int32_t kStopCircuit = int32_t(ROUTE_N) * kStopMm;

inline int32_t stopRouteMod(int64_t value, int32_t length) {
  value %= length;
  return int32_t(value < 0 ? value + length : value);
}

enum class StopSection : uint8_t { Outside, Approach, Fifty, Penultimate, Final };

struct StopGeography {
  StopSection section = StopSection::Outside;
  uint8_t pwm = 90;
  int32_t remainingMm = 0;
  bool applicable() const { return section != StopSection::Outside; }
};

// Copied unchanged from NAVI_EWO_0_6 NaviStopOverlay.h.
inline StopGeography stopGeography(int32_t position, int32_t target, int8_t direction) {
  StopGeography out;
  if (direction != 1 && direction != -1) return out;
  const int32_t d = stopRouteMod(int64_t(direction) * (target - position), kStopCircuit);
  out.remainingMm = d;
  if (d > 11 * kStopMm) return out;
  if (d > 7 * kStopMm) {
    out.section = StopSection::Approach;
    out.pwm = uint8_t(50 + (10 * (d - 7 * kStopMm) + kStopMm - 1) / kStopMm);
  } else if (d > 3 * kStopMm / 2) {
    out.section = StopSection::Fifty;
    out.pwm = 50;
  } else if (d > kStopMm / 2) {
    out.section = StopSection::Penultimate;
    out.pwm = uint8_t(35 + (15 * (d - kStopMm / 2) + kStopMm - 1) / kStopMm);
  } else {
    out.section = StopSection::Final;
    out.pwm = uint8_t((70 * d + kStopMm - 1) / kStopMm);
  }
  return out;
}
inline StopGeography mmGeography(uint8_t positionMm, uint8_t targetMm, int8_t direction) {
  return stopGeography(int32_t(positionMm) * kStopMm, int32_t(targetMm) * kStopMm, direction);
}

enum class StopExecution : uint8_t { Geographic, Braking, Dwell, Released };

struct StopCommand {
  uint8_t pwm = 90;
  bool overlay = false;
  bool releasedNow = false;
  StopExecution execution = StopExecution::Geographic;
};

// Copied from NAVI_EWO_0_6 NaviStopOverlay (behaviour unchanged).
class NaviStopOverlay {
 public:
  StopExecution execution() const { return execution_; }
  void reset() {
    execution_ = StopExecution::Geographic;
    zeroTiming_ = wasEnabled_ = geographicOverlay_ = false;
    direction_ = 0;
  }
  StopCommand command(const StopGeography& geo, bool geographyAvailable,
                      int8_t direction, uint8_t cruise, uint8_t appliedPwm,
                      uint8_t currentCommand, uint32_t nowMs, bool enabled) {
    StopCommand out;
    out.pwm = cruise;
    if (direction != direction_) { reset(); direction_ = direction; }
    if (appliedPwm != 0) zeroTiming_ = false;
    if (appliedPwm == 0 && (execution_ == StopExecution::Braking ||
                           execution_ == StopExecution::Dwell) && !zeroTiming_) {
      zeroTiming_ = true; zeroSinceMs_ = nowMs; execution_ = StopExecution::Dwell;
    }
    if (execution_ == StopExecution::Released && geographyAvailable && !geo.applicable()) {
      execution_ = StopExecution::Geographic; zeroTiming_ = false;
    }
    if (!enabled) {
      wasEnabled_ = false; out.pwm = currentCommand; out.execution = execution_; return out;
    }
    if (!wasEnabled_ && geographyAvailable && geo.section != StopSection::Final &&
        execution_ != StopExecution::Released) {
      execution_ = StopExecution::Geographic; zeroTiming_ = false;
    }
    wasEnabled_ = true;
    if (execution_ == StopExecution::Released) { out.execution = execution_; return out; }
    if (execution_ == StopExecution::Geographic && geographyAvailable &&
        geo.section == StopSection::Final)
      execution_ = StopExecution::Braking;
    if (execution_ == StopExecution::Braking || execution_ == StopExecution::Dwell) {
      out.overlay = true; out.pwm = 0;
      if (appliedPwm == 0) {
        if (!zeroTiming_) { zeroTiming_ = true; zeroSinceMs_ = nowMs; }
        execution_ = StopExecution::Dwell;
        if (uint32_t(nowMs - zeroSinceMs_) >= STOP_DWELL_MS) {
          execution_ = StopExecution::Released; out.overlay = false; out.releasedNow = true; out.pwm = cruise;
        }
      } else execution_ = StopExecution::Braking;
    } else if (geographyAvailable) {
      out.overlay = geo.applicable();
      out.pwm = out.overlay ? geo.pwm : cruise;
    } else {
      out.pwm = currentCommand; out.overlay = geographicOverlay_;
    }
    geographicOverlay_ = out.overlay;
    out.execution = execution_;
    return out;
  }
 private:
  StopExecution execution_ = StopExecution::Geographic;
  bool zeroTiming_ = false, wasEnabled_ = false, geographicOverlay_ = false;
  int8_t direction_ = 0;
  uint32_t zeroSinceMs_ = 0;
};

// ------------------------------------------------------------------ stations --
struct Station { const char* name; uint8_t mm; };
static constexpr uint8_t STATION_COUNT = 4;
static constexpr Station STATIONS[STATION_COUNT] = {
  {"PATIO", 15}, {"GRILLERS", 63}, {"ARCHES", 108}, {"BAMBOO", 157}
};
static constexpr uint8_t NO_STATION = 0xFF;

// MM travelled from `from` to reach `to` going `dir` (0..ROUTE_N-1).
inline uint8_t aheadMm(uint8_t from, uint8_t to, int8_t dir) {
  return dir > 0 ? routeMod(int32_t(to) - from) : routeMod(int32_t(from) - to);
}
// The MM `n` markers before `mm` along the approach in `dir`.
inline uint8_t before(uint8_t mm, int8_t dir, uint8_t n) { return routeMod(int32_t(mm) - int32_t(dir) * n); }
// Nearest station at or ahead of `pos` in `dir`.
inline uint8_t nearestStationAhead(uint8_t pos, int8_t dir) {
  uint8_t best = 0, bestD = 0xFF;
  for (uint8_t i = 0; i < STATION_COUNT; ++i) {
    const uint8_t d = aheadMm(pos, STATIONS[i].mm, dir);
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

// ---------------------------------------------------------------------- peer --
// Dynamic roles (operator, 2026-10-20). No fixed leader: from its own committed
// MM and the other's broadcast MM, each locomotive derives its role with the
// same rule: the other is AHEAD within the shorter way round the loop => I am
// the FOLLOWER, otherwise the LEADER (ROUTE_N is odd, so there is no tie; the
// same MM => no role). Each broadcasts the role it derived (the role echo); a
// role is CONFIRMED only while the other broadcasts the opposite role.
enum class Role : uint8_t { None = 0, Leader, Follower };
inline const char* roleName(Role r) { return r == Role::Leader ? "LEADER" : r == Role::Follower ? "FOLLOWER" : "NONE"; }
inline Role opposite(Role r) { return r == Role::Leader ? Role::Follower : r == Role::Follower ? Role::Leader : Role::None; }
inline Role deriveRole(uint8_t pos, uint8_t peerPos, int8_t dir) {
  if (pos == peerPos) return Role::None;
  return aheadMm(pos, peerPos, dir) <= ROUTE_N / 2 ? Role::Follower : Role::Leader;
}

// What this locomotive broadcasts (ESP-NOW, 4 Hz).
struct __attribute__((packed)) PeerPacket {
  uint8_t magic = 0xB7, version = 3;
  uint32_t locoId = 0, seq = 0;
  uint8_t reliable = 0;      // TRACKING with a committed MM
  int8_t dir = 0;            // +1 CW, -1 CCW, 0 unset
  uint8_t lastMm = 0;        // committed MM
  uint8_t station = NO_STATION;   // index of the station it is working
  uint8_t execution = 0;     // StopExecution at that station
  uint8_t autoRunning = 0;
  uint8_t bubble = 0;        // BubbleState
  uint8_t role = 0;          // Role it derived (role echo)
  uint8_t pwm = 0;           // applied PWM (0 = standing): the follower sees the leader start
};
static_assert(sizeof(PeerPacket) == 19, "PeerPacket layout");

struct PeerView {
  bool everSeen = false;     // a peer packet has been received this boot
  uint32_t ageMs = 0;        // since the last one
  PeerPacket p;
};

// PAIRING. Two locomotives are paired once one hears the other, fresh and
// TRACKING, while it is itself TRACKING. Never paired = running singly: no
// bubble, whatever else is on the radio. A new declaration ends pairing
// (StationDriver::reset). LINK while paired: KNOWN = fresh, tracking, same
// direction, different MM, and roles confirmed; anything else = LOST.
enum class Link : uint8_t { Solo = 0, Known, Lost };
inline const char* linkName(Link l) { return l == Link::Solo ? "SOLO" : l == Link::Known ? "KNOWN" : "LOST"; }

enum class Occupancy : uint8_t { Free = 0, Occupied, NoPeer, Lost };
inline const char* occupancyName(Occupancy o) {
  switch (o) { case Occupancy::Free: return "FREE"; case Occupancy::Occupied: return "OCCUPIED";
               case Occupancy::Lost: return "PEER_LOST"; default: return "NO_PEER"; }
}

// Is the leader stopped at station `si` (final braking, dwell, or waiting for
// us), as seen from `pos` travelling `dir`? Only asked with a KNOWN link. The
// leader must be AHEAD of us, within the carpet beyond the platform (it may
// have rolled past the centre MM). Operator 2026-10-20: the follower's hold
// applies only if the leader is stopped at the station, braking included.
inline Occupancy stationOccupancy(const PeerPacket& p, uint8_t si, uint8_t pos, int8_t dir) {
  const uint8_t toStation = aheadMm(pos, STATIONS[si].mm, dir);
  const uint8_t toPeer = aheadMm(pos, p.lastMm, dir);
  const StopExecution ex = StopExecution(p.execution);
  if (p.station == si && (ex == StopExecution::Braking || ex == StopExecution::Dwell) &&
      toPeer >= 1 && toPeer <= toStation + 11) return Occupancy::Occupied;
  return Occupancy::Free;
}

// Has the follower arrived? It is stopped at the holding point of our station.

// ------------------------------------------------------------------- driver --
// CONVOY (operator 2026-10-20, "Always"): a confirmed LEADER does not leave the
// platform until the follower has arrived: stopped (applied PWM 0) at the
// holding point of this station. Then the leader starts; the follower moves up
// a random 2-5 s later. The leader starts a random 0-3 s after the follower
// stands at PWM 0. Unpaired: no wait.
// Target kinds: PLATFORM (the station stop); HOLD (bubble: the same carpet 5 MM
// earlier); STOP_NOW (bubble: occupied when already inside 5 MM: stop at once,
// fast ramp); LOST_STOP (paired peer lost, follower: a carpet stop at the next MM).
// SILENCE RULE (operator, 2026-10-20): paired and the link is LOST =>
//   LEADER (last confirmed role): go on to the next station, make the platform
//     stop, and stay there;
//   FOLLOWER (or never confirmed): initiate a rainbow-carpet stop at once.
//   Either way a stop already under way is completed and held. Both resume
//   when the link is KNOWN again.
enum class TargetKind : uint8_t { Platform = 0, Hold, StopNow, LostStop };
enum class BubbleState : uint8_t { None = 0, Approaching, Holding };
// Has the follower arrived? It is stopped at the holding point of our station.
inline bool followerArrived(const PeerPacket& p, uint8_t si) {
  return p.station == si && BubbleState(p.bubble) == BubbleState::Holding;
}
inline const char* targetKindName(TargetKind k) {
  switch (k) { case TargetKind::Platform: return "PLATFORM"; case TargetKind::Hold: return "HOLD";
               case TargetKind::StopNow: return "STOP_NOW"; default: return "LOST_STOP"; }
}
inline const char* executionName(StopExecution e) {
  switch (e) { case StopExecution::Geographic: return "GEOGRAPHIC"; case StopExecution::Braking: return "BRAKING";
               case StopExecution::Dwell: return "DWELL"; default: return "RELEASED"; }
}
inline const char* sectionName(StopSection s) {
  switch (s) { case StopSection::Approach: return "APPROACH"; case StopSection::Fifty: return "FIFTY";
               case StopSection::Penultimate: return "PENULTIMATE"; case StopSection::Final: return "FINAL";
               default: return "OUTSIDE"; }
}

struct DriveInput {
  bool reliable = false;     // navigator TRACKING
  uint8_t pos = 0;           // committed MM
  int8_t dir = 0;
  uint8_t cruise = 90;       // grade-aware base cruise at pos
  uint8_t appliedPwm = 0, commandedPwm = 0;
  uint32_t nowMs = 0;
  bool enabled = false;      // AUTO running
  PeerView peer;
};
struct DriveOutput {
  StopCommand cmd;
  uint8_t station = NO_STATION;
  TargetKind kind = TargetKind::Platform;
  uint8_t targetMm = 0;
  StopGeography geo;
  Occupancy occupancy = Occupancy::NoPeer;
  BubbleState bubble = BubbleState::None;
  Link link = Link::Solo;
  Role role = Role::None;            // derived now (broadcast as the echo)
  Role confirmedRole = Role::None;   // last role the other confirmed
  bool paired = false, platformHeld = false;
  bool followHeld = false, followCapped = false;   // follow-the-leader carpet
  uint8_t gapToPeer = 0xFF;                        // MM to the leader ahead (follower only)
  bool targetChanged = false;
  bool urgent = false;       // STOP_NOW: brake at the fast AUTO ramp, not the gentle station ramp
};

class StationDriver {
 public:
  void reset() {
    overlay_.reset(); station_ = NO_STATION; kind_ = TargetKind::Platform; target_ = 0; dir_ = 0;
    held_ = platformHeld_ = paired_ = freeTiming_ = false; lastRole_ = Role::None;
    followHeld_ = peerMoved_ = arrivalTiming_ = false;
  }
  void seed(uint32_t s) { rng_ = s ? s : 0x9E3779B9u; }
  uint8_t station() const { return station_; }
  StopExecution execution() const { return overlay_.execution(); }

  DriveOutput step(const DriveInput& in) {
    DriveOutput out;
    // ---- the other locomotive
    const PeerView& v = in.peer;
    const bool fresh = v.everSeen && v.ageMs <= PEER_STALE_MS && v.p.reliable;
    if (fresh && in.reliable) paired_ = true;
    const bool comparable = fresh && in.reliable && v.p.dir == in.dir && (in.dir == 1 || in.dir == -1);
    const Role role = comparable ? deriveRole(in.pos, v.p.lastMm, in.dir) : Role::None;
    const bool confirmed = role != Role::None && Role(v.p.role) == opposite(role);
    if (confirmed) lastRole_ = role;
    const Link link = !paired_ ? Link::Solo : confirmed ? Link::Known : Link::Lost;
    out.link = link; out.role = role; out.confirmedRole = lastRole_; out.paired = paired_;

    if (!in.reliable || (in.dir != 1 && in.dir != -1)) {
      out.cmd = overlay_.command(StopGeography{}, false, in.dir, in.cruise, in.appliedPwm, in.commandedPwm, in.nowMs, in.enabled);
      out.station = station_; return out;
    }
    // Station selection (EWO NaviStations::update, whole MM): keep the station
    // through braking and dwell, and after release while its footprint lasts.
    const StopExecution ex = overlay_.execution();
    bool keep = station_ != NO_STATION && in.dir == dir_;
    if (keep && ex == StopExecution::Released) {
      keep = platformHeld_ || mmGeography(in.pos, STATIONS[station_].mm, in.dir).applicable();
    } else if (keep && (ex == StopExecution::Geographic) && kind_ == TargetKind::Platform) {
      keep = nearestStationAhead(in.pos, in.dir) == station_;
    }
    if (!keep) {
      station_ = nearestStationAhead(in.pos, in.dir); dir_ = in.dir;
      setTarget(TargetKind::Platform, STATIONS[station_].mm, out);
    }
    const uint8_t s = STATIONS[station_].mm;
    const uint8_t toStation = aheadMm(in.pos, s, in.dir);
    // CONVOY: the follower has arrived (stands at PWM 0 behind us at this
    // station); we may start a random LEADER_START_DELAY after that.
    bool convoyReady = false;
    if (link == Link::Known && lastRole_ == Role::Leader && followerArrived(v.p, station_)) {
      if (!arrivalTiming_) { arrivalTiming_ = true; arrivalMs_ = in.nowMs; leaderDelayMs_ = randRange(LEADER_START_DELAY_MIN_MS, LEADER_START_DELAY_MAX_MS); }
      convoyReady = uint32_t(in.nowMs - arrivalMs_) >= leaderDelayMs_;
    } else arrivalTiming_ = false;
    const bool convoyWaitNow = link == Link::Known && lastRole_ == Role::Leader && !convoyReady;
    const bool platformStopUnderway = kind_ == TargetKind::Platform &&
        (ex == StopExecution::Braking || ex == StopExecution::Dwell || platformHeld_);
    const bool nonPlatformStopUnderway = kind_ != TargetKind::Platform && held_;

    if (link == Link::Lost) {
      out.occupancy = Occupancy::Lost;
      const bool leader = lastRole_ == Role::Leader;
      if (!platformStopUnderway && !nonPlatformStopUnderway) {
        if (leader) { if (kind_ != TargetKind::Platform) setTarget(TargetKind::Platform, s, out); }
        else if (kind_ != TargetKind::LostStop) setTarget(TargetKind::LostStop, navi_one::nextMarker(in.pos, in.dir), out);
      }
    } else {
      out.occupancy = link == Link::Known ? stationOccupancy(v.p, station_, in.pos, in.dir) : Occupancy::NoPeer;
      // A held platform may release unless we are the confirmed leader and the follower has not arrived.
      if (!convoyWaitNow) platformHeld_ = false;
      const bool blocked = out.occupancy == Occupancy::Occupied;
      if (blocked && !platformStopUnderway && toStation <= 11 + HOLD_TARGET_MM) {
        if (kind_ == TargetKind::Platform) {
          // The same carpet, HOLD_TARGET_MM earlier (stands ~BUBBLE_HOLD_MM short); inside that: stop here.
          if (toStation > HOLD_TARGET_MM) setTarget(TargetKind::Hold, before(s, in.dir, HOLD_TARGET_MM), out);
          else setTarget(TargetKind::StopNow, in.pos, out);
        }
      } else if (!blocked && kind_ != TargetKind::Platform) {
        // The leader has left the platform / the peer is back. Stopped at the
        // holding point: wait a random 2-5 s, then move up.
        if (!held_) setTarget(TargetKind::Platform, s, out);
        else {
          if (!freeTiming_) { freeTiming_ = true; freeSinceMs_ = in.nowMs; holdDelayMs_ = randRange(FOLLOWER_START_DELAY_MIN_MS, FOLLOWER_START_DELAY_MAX_MS); }
          if (uint32_t(in.nowMs - freeSinceMs_) >= holdDelayMs_) setTarget(TargetKind::Platform, s, out);
        }
      }
      if (blocked) freeTiming_ = false;
    }

    if (held_ || platformHeld_) {                    // stopped and waiting: stay at zero
      out.geo = mmGeography(in.pos, target_, in.dir);
      out.cmd = StopCommand{}; out.cmd.pwm = 0; out.cmd.overlay = true; out.cmd.execution = StopExecution::Dwell;
    } else {
      out.geo = mmGeography(in.pos, target_, in.dir);
      if (kind_ == TargetKind::StopNow) { out.geo = StopGeography{}; out.geo.section = StopSection::Final; out.geo.pwm = 0; }
      out.cmd = overlay_.command(out.geo, true, in.dir, in.cruise, in.appliedPwm, in.commandedPwm, in.nowMs, in.enabled);
      if (kind_ != TargetKind::Platform && (out.cmd.execution == StopExecution::Braking ||
          out.cmd.execution == StopExecution::Dwell || out.cmd.execution == StopExecution::Released)) {
        held_ = true;                                // latched until released by the peer
        out.cmd = StopCommand{}; out.cmd.pwm = 0; out.cmd.overlay = true; out.cmd.execution = StopExecution::Braking;
      } else if (kind_ == TargetKind::Platform &&
                 (link == Link::Lost || convoyWaitNow) &&
                 (out.cmd.releasedNow || (out.cmd.execution == StopExecution::Released && in.appliedPwm == 0))) {
        platformHeld_ = true;                        // peer lost, or (convoy) the follower has not arrived: stay
        out.cmd = StopCommand{}; out.cmd.pwm = 0; out.cmd.overlay = true; out.cmd.execution = StopExecution::Dwell;
      }
    }
    // ---- FOLLOW THE LEADER, anywhere (operator 2026-10-20: "The locomotives
    // must know where the other locomotive is located"). The same rainbow
    // carpet, anchored BUBBLE_HOLD_MM behind the leader's committed MM, caps
    // our PWM; at its end we stop and stay until the leader has moved on, then
    // move up after a random 2-5 s. The slower of station and follow wins.
    out.gapToPeer = 0xFF;
    if (link == Link::Known && lastRole_ == Role::Follower && role == Role::Follower) {
      const uint8_t gap = aheadMm(in.pos, v.p.lastMm, in.dir);
      out.gapToPeer = gap;
      const int dLead = int(gap) - int(BUBBLE_HOLD_MM);
      if (followHeld_) {
        // The leader has started: its applied PWM is up, or its committed MM has moved on.
        const bool moved = v.p.pwm > 0 || (v.p.lastMm != heldPeerMm_ && aheadMm(heldPeerMm_, v.p.lastMm, in.dir) <= ROUTE_N / 2);
        if (moved && !peerMoved_) { peerMoved_ = true; moveSeenMs_ = in.nowMs; followDelayMs_ = randRange(FOLLOWER_START_DELAY_MIN_MS, FOLLOWER_START_DELAY_MAX_MS); }
        if (!moved && v.p.pwm == 0 && v.p.lastMm == heldPeerMm_) peerMoved_ = false;   // it stopped again before we left
        // Move up after the delay AND only once the leader's committed MM has moved
        // on far enough (dLead > 0). Throttle alone is never movement (field
        // 2026-10-20, Arches 17:21: the leader's PWM was up, its position was not
        // moving, the follower crept into it).
        if (peerMoved_ && uint32_t(in.nowMs - moveSeenMs_) >= followDelayMs_ && dLead > 0) followHeld_ = false;
      } else if (dLead <= 0) {
        followHeld_ = true; heldPeerMm_ = v.p.lastMm; peerMoved_ = false;   // within 5 MM of the leader: stop
      }
      if (followHeld_) {
        out.followHeld = true;
        if (out.cmd.pwm != 0 || out.cmd.execution == StopExecution::Geographic || out.cmd.execution == StopExecution::Released) {
          out.cmd.pwm = 0; out.cmd.overlay = true; out.cmd.execution = StopExecution::Braking;
        }
      } else if (dLead <= 11) {
        const StopGeography g = stopGeography(0, int32_t(dLead) * kStopMm, 1);
        if (g.applicable() && g.pwm < out.cmd.pwm) { out.cmd.pwm = g.pwm; out.cmd.overlay = true; out.followCapped = true; }
      }
    } else { followHeld_ = false; peerMoved_ = false; }

    out.station = station_; out.kind = kind_; out.targetMm = target_; out.platformHeld = platformHeld_;
    out.urgent = kind_ == TargetKind::StopNow;
    // HOLDING = stopped and waiting (applied PWM 0). Operator 2026-10-20: the
    // leader departs when the follower stops (PWM = 0), not when it starts braking.
    const bool waiting = held_ || platformHeld_ || followHeld_;
    out.bubble = (kind_ == TargetKind::Platform && !platformHeld_ && !followHeld_) ? BubbleState::None
               : (waiting && in.appliedPwm == 0 ? BubbleState::Holding : BubbleState::Approaching);
    return out;
  }

 private:
  NaviStopOverlay overlay_;
  uint8_t station_ = NO_STATION, target_ = 0; int8_t dir_ = 0;
  bool held_ = false, platformHeld_ = false, paired_ = false, freeTiming_ = false;
  uint32_t freeSinceMs_ = 0, holdDelayMs_ = FOLLOWER_START_DELAY_MIN_MS;
  bool followHeld_ = false, peerMoved_ = false, arrivalTiming_ = false;
  uint8_t heldPeerMm_ = 0;
  uint32_t moveSeenMs_ = 0, followDelayMs_ = FOLLOWER_START_DELAY_MIN_MS, arrivalMs_ = 0, leaderDelayMs_ = 0;
  uint32_t rng_ = 0x9E3779B9u;
  uint32_t randRange(uint32_t lo, uint32_t hi) {           // xorshift32, inclusive range
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return hi > lo ? lo + rng_ % (hi - lo + 1) : lo;
  }
  Role lastRole_ = Role::None;
  TargetKind kind_ = TargetKind::Platform;
  void setTarget(TargetKind k, uint8_t mm, DriveOutput& out) {
    if (k == kind_ && mm == target_ && station_ != NO_STATION && !out.targetChanged) return;
    kind_ = k; target_ = mm; overlay_.reset(); held_ = false; freeTiming_ = false; out.targetChanged = true;
  }
};

}  // namespace rvm_ops
