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
//   it, or it is past the station).
//   Unknown peer (heard before but silent for PEER_STALE_MS, not tracking, or
//   travelling the other way): it may be standing anywhere ahead, so we stop
//   where we are (STOP_NOW) and wait until it is known again. A peer never
//   heard = no other train: no bubble. Consequence: both trains must be
//   declared before GO (a powered, undeclared peer is Unknown).
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

static constexpr uint8_t BUBBLE_HOLD_MM = 5;       // operator 2026-10-20; adjust here
static constexpr uint32_t PEER_STALE_MS = 2000;     // 8 missed 4 Hz packets
static constexpr uint32_t STOP_DWELL_MS = 5000;     // EWO_0_6 kStopDwellMs

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
// What this locomotive broadcasts (ESP-NOW, 4 Hz) and what it knows of the other.
struct __attribute__((packed)) PeerPacket {
  uint8_t magic = 0xB7, version = 1;
  uint32_t locoId = 0, seq = 0;
  uint8_t reliable = 0;      // TRACKING with a committed MM
  int8_t dir = 0;            // +1 CW, -1 CCW, 0 unset
  uint8_t lastMm = 0;        // committed MM
  uint8_t station = NO_STATION;   // index of the station it is working
  uint8_t execution = 0;     // StopExecution at that station
  uint8_t autoRunning = 0;
  uint8_t bubble = 0;        // BubbleState
};
static_assert(sizeof(PeerPacket) == 17, "PeerPacket layout");

struct PeerView {
  bool everSeen = false;     // a peer packet has been received this boot
  uint32_t ageMs = 0;        // since the last one
  PeerPacket p;
};

enum class Occupancy : uint8_t { Free = 0, Occupied, Unknown, NoPeer };
inline const char* occupancyName(Occupancy o) {
  switch (o) { case Occupancy::Free: return "FREE"; case Occupancy::Occupied: return "OCCUPIED";
               case Occupancy::Unknown: return "UNKNOWN"; default: return "NO_PEER"; }
}

// Is the leader in station `si`, as seen from `pos` travelling `dir`? The
// leader must be AHEAD of us (same MM = cannot tell who leads = Unknown).
inline Occupancy stationOccupancy(const PeerView& v, uint8_t si, uint8_t pos, int8_t dir) {
  if (!v.everSeen) return Occupancy::NoPeer;
  if (v.ageMs > PEER_STALE_MS || !v.p.reliable || v.p.dir != dir) return Occupancy::Unknown;
  if (v.p.lastMm == pos) return Occupancy::Unknown;
  const uint8_t s = STATIONS[si].mm;
  const uint8_t toStation = aheadMm(pos, s, dir);
  const uint8_t toPeer = aheadMm(pos, v.p.lastMm, dir);
  const bool working = v.p.station == si;
  const StopExecution ex = StopExecution(v.p.execution);
  // Braking or dwelling for this platform, anywhere ahead of us within the
  // carpet beyond it (it may have rolled past the centre MM): occupied.
  if (working && (ex == StopExecution::Braking || ex == StopExecution::Dwell) &&
      toPeer <= toStation + 11) return Occupancy::Occupied;
  if (toPeer > toStation + 2) return Occupancy::Free;                 // beyond the platform, or behind us
  const uint8_t hold = before(s, dir, BUBBLE_HOLD_MM);
  const uint8_t into = aheadMm(hold, v.p.lastMm, dir);                // 0 = at the holding point
  const bool between = into >= 1 && into <= BUBBLE_HOLD_MM;           // (hold, station]
  const bool releasedHere = working && ex == StopExecution::Released;
  if (between && !releasedHere) return Occupancy::Occupied;
  return Occupancy::Free;
}

// ------------------------------------------------------------------- driver --
enum class TargetKind : uint8_t { Platform = 0, Hold, StopNow };
enum class BubbleState : uint8_t { None = 0, Approaching, Holding };
inline const char* targetKindName(TargetKind k) {
  return k == TargetKind::Platform ? "PLATFORM" : k == TargetKind::Hold ? "HOLD" : "STOP_NOW";
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
  bool targetChanged = false;
  bool urgent = false;       // STOP_NOW: brake at the fast AUTO ramp, not the gentle station ramp
};

class StationDriver {
 public:
  void reset() { overlay_.reset(); station_ = NO_STATION; kind_ = TargetKind::Platform; target_ = 0; dir_ = 0; held_ = false; }
  uint8_t station() const { return station_; }
  StopExecution execution() const { return overlay_.execution(); }

  DriveOutput step(const DriveInput& in) {
    DriveOutput out;
    if (!in.reliable || (in.dir != 1 && in.dir != -1)) {
      out.cmd = overlay_.command(StopGeography{}, false, in.dir, in.cruise, in.appliedPwm, in.commandedPwm, in.nowMs, in.enabled);
      out.station = station_; return out;
    }
    // Station selection (EWO NaviStations::update, whole MM): keep the station
    // through braking and dwell, and after release while its footprint lasts.
    const StopExecution ex = overlay_.execution();
    bool keep = station_ != NO_STATION && in.dir == dir_;
    if (keep && ex == StopExecution::Released) {
      keep = mmGeography(in.pos, STATIONS[station_].mm, in.dir).applicable();
    } else if (keep && (ex == StopExecution::Geographic)) {
      keep = nearestStationAhead(in.pos, in.dir) == station_;
    }
    if (!keep) {
      station_ = nearestStationAhead(in.pos, in.dir); dir_ = in.dir;
      setTarget(TargetKind::Platform, STATIONS[station_].mm, out);
    }
    const uint8_t s = STATIONS[station_].mm;
    out.occupancy = stationOccupancy(in.peer, station_, in.pos, in.dir);
    const bool blocked = out.occupancy == Occupancy::Occupied || out.occupancy == Occupancy::Unknown;
    const uint8_t toStation = aheadMm(in.pos, s, in.dir);
    const bool platformStopUnderway = kind_ == TargetKind::Platform &&
        (ex == StopExecution::Braking || ex == StopExecution::Dwell || ex == StopExecution::Released);

    if (out.occupancy == Occupancy::Unknown && !platformStopUnderway) {
      // The other train's position is not known (silent, untracked, other
      // direction): it may be standing anywhere ahead. Stop where we are.
      if (kind_ != TargetKind::StopNow) setTarget(TargetKind::StopNow, in.pos, out);
    } else if (blocked && !platformStopUnderway && toStation <= 11 + BUBBLE_HOLD_MM) {
      if (kind_ == TargetKind::Platform) {
        // The same carpet, BUBBLE_HOLD_MM earlier; inside that distance: stop here.
        if (toStation > BUBBLE_HOLD_MM) setTarget(TargetKind::Hold, before(s, in.dir, BUBBLE_HOLD_MM), out);
        else setTarget(TargetKind::StopNow, in.pos, out);
      }
    } else if (!blocked && kind_ != TargetKind::Platform) {
      setTarget(TargetKind::Platform, s, out);       // the leader has left the platform
    }

    if (held_) {                                     // stopped short of an occupied platform: stay at zero
      out.geo = mmGeography(in.pos, target_, in.dir);
      out.cmd = StopCommand{}; out.cmd.pwm = 0; out.cmd.overlay = true; out.cmd.execution = StopExecution::Dwell;
    } else {
      out.geo = mmGeography(in.pos, target_, in.dir);
      if (kind_ == TargetKind::StopNow) { out.geo = StopGeography{}; out.geo.section = StopSection::Final; out.geo.pwm = 0; }
      out.cmd = overlay_.command(out.geo, true, in.dir, in.cruise, in.appliedPwm, in.commandedPwm, in.nowMs, in.enabled);
      if (kind_ != TargetKind::Platform && (out.cmd.execution == StopExecution::Braking ||
          out.cmd.execution == StopExecution::Dwell || out.cmd.execution == StopExecution::Released)) {
        held_ = true;                                // latched until the platform is free
        out.cmd = StopCommand{}; out.cmd.pwm = 0; out.cmd.overlay = true; out.cmd.execution = StopExecution::Braking;
      }
    }
    out.station = station_; out.kind = kind_; out.targetMm = target_;
    out.urgent = kind_ == TargetKind::StopNow;
    out.bubble = kind_ == TargetKind::Platform ? BubbleState::None
               : (held_ ? BubbleState::Holding : BubbleState::Approaching);
    return out;
  }

 private:
  NaviStopOverlay overlay_;
  uint8_t station_ = NO_STATION, target_ = 0; int8_t dir_ = 0; bool held_ = false;
  TargetKind kind_ = TargetKind::Platform;
  void setTarget(TargetKind k, uint8_t mm, DriveOutput& out) {
    if (k == kind_ && mm == target_ && station_ != NO_STATION && !out.targetChanged) { kind_ = k; target_ = mm; return; }
    kind_ = k; target_ = mm; overlay_.reset(); held_ = false; out.targetChanged = true;
  }
};

}  // namespace rvm_ops
