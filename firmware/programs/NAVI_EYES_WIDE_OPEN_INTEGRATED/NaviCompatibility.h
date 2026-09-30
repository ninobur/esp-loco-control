#pragma once
#include <stdio.h>
#include "NaviIntegratedCore.h"

namespace navi_eyes {
// Output-only adapter. Console vocabulary is not navigation authority.
inline int formatConsoleNav(char* out, size_t size, const NaviIntegratedCore& n,
                            int8_t sessionDir, uint64_t nowUs) {
  const bool usable = n.declared() && n.positionReliable();
  return snprintf(out, size,
    "{\"state\":\"%s\",\"mm\":%u,\"target\":%u,\"dir\":%d,"
    "\"session_dir\":\"%s\",\"position_reliable\":%u,"
    "\"ir_distance_state\":\"%s\","
    "\"reference_ready\":%u,\"boot_reference\":\"%s\","
    "\"authority\":\"NAVI_EWO\"}",
    usable ? "NORMAL" : "UNSET", n.mm(), unsigned(n.target().sequence), n.direction(),
    sessionDir > 0 ? "CW" : sessionDir < 0 ? "CCW" : "UNSET", usable,
    n.irDistanceState(nowUs), n.initialReferenceReady(),
    n.initialReferenceReady() ? "PROVISIONAL_OR_SPATIAL" : "UNSET");
}
inline int formatConsoleIr(char* out, size_t size, const NaviIntegratedCore& n,
                           uint64_t nowUs, bool coupled) {
  char speed[40] = "null";
  const bool valid = n.irSpeedAvailable(nowUs);
  if (valid) snprintf(speed, sizeof(speed), "%.3f", n.irSpeedMmS());
  return snprintf(out, size,
    "{\"ir_valid\":%u,\"ir_mmps\":%s,\"ir_coupled\":%u,"
    "\"ir_speed_reason\":\"%s\",\"ir_applicable\":%u,\"ir_reason\":%u,"
    "\"ir_seq\":%lu,\"ir_pulses\":%llu,\"ir_gap\":%llu,\"ir_sat\":%llu,"
    "\"ir_abort\":%llu,\"authority\":\"NAVI_EWO\"}",
    valid, speed, coupled, valid ? (n.irSpeedMmS() == 0 ? "STOPPED" : "MEASURED") : "UNAVAILABLE",
    n.irApplicable(nowUs),
    n.latestIr().opticalReason, (unsigned long)n.latestIr().sequence,
    (unsigned long long)n.latestIr().completedPulses,
    (unsigned long long)n.latestIr().sampleGaps,
    (unsigned long long)n.latestIr().saturatedSamples,
    (unsigned long long)n.latestIr().openAborts);
}
} // namespace navi_eyes
