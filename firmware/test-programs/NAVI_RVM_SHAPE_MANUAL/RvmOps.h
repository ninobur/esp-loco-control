#pragma once
// ---------------------------------------------------------------------------
// RvmOps — payload parsing and MANUAL admissibility. Pure functions, no state.
// The parsers are copied unchanged from NAVI_RVM's Ops.h (d18fe67), including
// its two rules: a payload that is not understood is refused, and on an
// emergency topic ambiguity means STOP. The AUTO/GO/station admissibility and
// StopArmingPolicy are not carried: this sketch has no automatic operation.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>

namespace navi_one {

// --- parsing ---------------------------------------------------------------

// Fully numeric, optionally signed, no trailing rubbish, non-empty.
inline bool parseInt(const char* s, int& out) {
  if (!s || !*s) return false;
  const char* p = s;
  while (*p == ' ') ++p;
  bool neg = false;
  if (*p == '+' || *p == '-') { neg = (*p == '-'); ++p; }
  if (!*p) return false;
  long v = 0; int digits = 0;
  for (; *p >= '0' && *p <= '9'; ++p) {
    v = v * 10 + (*p - '0'); if (++digits > 9) return false;
  }
  if (!digits) return false;
  while (*p == ' ') ++p;
  if (*p) return false;
  out = (int)(neg ? -v : v);
  return true;
}

inline bool ieq(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b) {
    char ca = *a, cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
    if (ca != cb) return false;
  }
  return !*a && !*b;
}

// "1"/"0"/"true"/"false"/"on"/"off"/"yes"/"no". Anything else: false return.
inline bool parseBool(const char* s, bool& out) {
  if (!s) return false;
  if (ieq(s,"1")||ieq(s,"true")||ieq(s,"on")||ieq(s,"yes"))  { out = true;  return true; }
  if (ieq(s,"0")||ieq(s,"false")||ieq(s,"off")||ieq(s,"no")) { out = false; return true; }
  return false;
}

// RULE 2. Unreadable input on an emergency topic asserts the stop.
// Returns false when the payload was not understood, having ALREADY set
// assert=true. The caller stops and says the payload was rubbish.
inline bool parseEstop(const char* s, bool& assertStop) {
  if (parseBool(s, assertStop)) return true;
  assertStop = true;
  return false;
}

inline bool parseSessionDir(const char* s, int8_t& out) {
  if (ieq(s,"CW"))  { out = +1; return true; }
  if (ieq(s,"CCW")) { out = -1; return true; }
  return false;
}

// The console's motor-direction encoding: 0 = REVERSE, 2 = FORWARD. 1 has
// never meant anything and is refused rather than treated as "not 2".
inline bool parseMotorDir(const char* s, bool& forward) {
  if (ieq(s,"0") || ieq(s,"rev") || ieq(s,"reverse")) { forward = false; return true; }
  if (ieq(s,"2") || ieq(s,"fwd") || ieq(s,"forward")) { forward = true;  return true; }
  return false;
}

// "AAA-BBB", geometric and ascending, as the console's slider produces.
inline bool parseInterval(const char* s, int& a, int& b) {
  if (!s) return false;
  const char* dash = strchr(s, '-');
  if (!dash || dash == s || !dash[1]) return false;
  char lhs[16];
  size_t n = (size_t)(dash - s);
  if (n >= sizeof(lhs)) return false;
  memcpy(lhs, s, n); lhs[n] = 0;
  return parseInt(lhs, a) && parseInt(dash + 1, b);
}

// --- admissibility (manual only) ---------------------------------------------
struct Ops {
  bool  estopped      = false;
  bool  lowVoltage    = false;
  int   actualPwm     = 0;
  int   commandedPwm  = 0;
  int8_t sessionDir   = 0;
  int   safeDirPwm    = 15;      // SAFE_DIRECTION_CHANGE_PWM
};
using Refusal = const char*;     // nullptr = admitted

// Position is declared standing still (unchanged rule from Ops.h).
inline Refusal admitDeclaration(const Ops& o) {
  if (o.actualPwm > 0 || o.commandedPwm > 0)  return "REFUSED: stop first — position is declared standing still";
  return nullptr;
}
inline Refusal admitStartMarker(const Ops& o) {
  if (Refusal r = admitDeclaration(o)) return r;
  if (o.sessionDir == 0)               return "REFUSED: set session_direction first";
  return nullptr;
}
inline Refusal admitThrottle(const Ops& o) {
  if (o.lowVoltage) return "THROTTLE REFUSED: low voltage";
  if (o.estopped)   return "THROTTLE REFUSED: e-stop is set";
  return nullptr;
}
inline Refusal admitMotorDirection(const Ops& o) {
  if (o.actualPwm > o.safeDirPwm ||
      o.commandedPwm > o.safeDirPwm)   return "DIRECTION REFUSED: too fast to reverse";
  return nullptr;
}

}  // namespace navi_one
