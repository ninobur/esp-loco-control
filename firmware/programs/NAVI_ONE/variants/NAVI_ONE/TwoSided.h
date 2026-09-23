#pragma once
// ---------------------------------------------------------------------------
// TWO-SIDED INTERPRETATION OF AN INTERRUPTED TRAVERSAL
//
// The operator, 2026-09-02:
//
//   "When you look at two halves of a Gaussian curve you think 'that's two
//    halves of a Gaussian curve'. That's why it does not meet up exactly. What
//    does Toby think when he sees the same thing?"
//
// and the constraint that comes with it:
//
//   "Preserve the existing Gaussian's mathematical meaning and change only how
//    an interrupted traversal is REPRESENTED."
//
//   Ordinary passage:    one Gaussian, one temporal width.
//   Interrupted passage: one physical Gaussian field, observed in TWO movement
//                        intervals, potentially with different temporal widths.
//
// THIS IS NOT TWO CHANCES TO FIND A MAGNET. The two segments must JOINTLY
// establish one event: one polarity, one compatible amplitude scale, one
// coherent rise-peak-fall ordering, no contradictory excursion or extra lobe,
// exactly one verdict and at most one advance.
//
// The morphology is unchanged. A Gaussian in distance is a Gaussian in sample
// index while the speed is constant, and the speed IS constant within each
// movement interval -- that is what makes an interval an interval. So each
// segment is fitted with the same Gaussian the recognizer has always used, and
// only the width is allowed to differ between them.
//
// THE FIT IS CLOSED-FORM. ln y of a Gaussian is a parabola in the index:
//
//     ln y = ln A - (i - c)^2 / (2 sigma^2)   =   a i^2 + b i + d
//
// so a weighted least-squares parabola on the logarithm recovers all three
// parameters at once, with no iteration and no seed:
//
//     sigma = sqrt(-1 / (2a))     c = -b / (2a)     A = exp(d - b^2 / (4a))
//
// and a < 0 is itself a test: it says the segment has the CONCAVE-IN-LOG
// curvature a Gaussian flank has. A straight ramp does not.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
// Included by MagnetRecognizer.h once struct Passage exists; not standalone.

namespace navi_one {

struct SideFit {
  bool  ok       = false;   // a Gaussian flank could be fitted at all
  float amp      = 0.0f;    // A, extrapolated apex height
  float centre   = 0.0f;    // c, apex position in this segment's own index
  float sigma    = 0.0f;    // this interval's temporal width
  float residual = 0.0f;    // normalised RMS against its own fitted Gaussian
  uint16_t n     = 0;       // samples that carried the fit
  float span     = 0.0f;    // (last - first) index of the fitted region
  float reach    = 0.0f;    // how far outside the segment the apex sits, in sigma
};

struct TwoSidedVerdict {
  bool  tested   = false;   // false = not an interrupted passage; abstained
  bool  accept   = false;
  const char* why = "";
  SideFit arrival, departure;
  float ampAgreement = 0.0f;   // |Aa - Ad| / max(Aa, Ad)
  float ampJoint     = 0.0f;   // the one amplitude scale, if they agree
};

// Weighted parabola on the logarithm. Weight by y so that the samples carrying
// the arc dominate and the ones near zero -- where ln is violently noisy --
// do not. Returns false if the segment has no concave curvature to fit.
inline bool fitHalfGaussian(const int16_t* y, uint16_t from, uint16_t to,
                            float floorFrac, int16_t peak, SideFit& f) {
  if (!y || to <= from + 3) return false;
  const float thr = floorFrac * (float)peak;
  double S = 0, Sx = 0, Sx2 = 0, Sx3 = 0, Sx4 = 0, Sl = 0, Slx = 0, Slx2 = 0;
  uint16_t used = 0; float first = 0, last = 0;
  for (uint16_t i = from; i < to; ++i) {
    const float v = (float)y[i];
    if (v <= thr || v <= 1.0f) continue;
    const double x = (double)i, w = (double)v, l = log((double)v);
    if (!used) first = (float)i;
    last = (float)i; ++used;
    S += w; Sx += w*x; Sx2 += w*x*x; Sx3 += w*x*x*x; Sx4 += w*x*x*x*x;
    Sl += w*l; Slx += w*l*x; Slx2 += w*l*x*x;
  }
  if (used < 5) return false;
  // Solve the 3x3 normal equations for a, b, d.
  const double m[3][4] = {
    { Sx4, Sx3, Sx2, Slx2 },
    { Sx3, Sx2, Sx,  Slx  },
    { Sx2, Sx,  S,   Sl   },
  };
  double A[3][4];
  for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j) A[i][j] = m[i][j];
  for (int col = 0; col < 3; ++col) {
    int piv = col;
    for (int r = col + 1; r < 3; ++r) if (fabs(A[r][col]) > fabs(A[piv][col])) piv = r;
    if (fabs(A[piv][col]) < 1e-12) return false;
    if (piv != col) for (int j = 0; j < 4; ++j) { double t = A[col][j]; A[col][j] = A[piv][j]; A[piv][j] = t; }
    for (int r = 0; r < 3; ++r) {
      if (r == col) continue;
      const double k = A[r][col] / A[col][col];
      for (int j = col; j < 4; ++j) A[r][j] -= k * A[col][j];
    }
  }
  const double a = A[0][3] / A[0][0], b = A[1][3] / A[1][1], d = A[2][3] / A[2][2];
  if (a >= -1e-9) return false;              // not concave in log: not a Gaussian flank
  const double sigma = sqrt(-1.0 / (2.0 * a));
  const double centre = -b / (2.0 * a);
  const double amp = exp(d - b * b / (4.0 * a));
  if (!(sigma > 0.0) || !(amp > 0.0) || !isfinite(sigma) || !isfinite(amp)) return false;

  // Normalised RMS against the fitted Gaussian, over the same samples, scored
  // exactly as MagnetRecognizer::fitResidual scores a whole passage.
  double se = 0.0; uint16_t n = 0;
  for (uint16_t i = from; i < to; ++i) {
    const float v = (float)y[i];
    if (v <= thr || v <= 1.0f) continue;
    const double dx = ((double)i - centre) / sigma;
    const double e = (double)v - amp * exp(-0.5 * dx * dx);
    se += e * e; ++n;
  }
  f.ok = true;
  f.amp = (float)amp; f.centre = (float)centre; f.sigma = (float)sigma;
  f.residual = (float)(sqrt(se / (double)n) / (amp > 1.0 ? amp : 1.0));
  f.n = n; f.span = last - first;
  const float edge = centre < first ? first : (centre > last ? last : (float)centre);
  f.reach = (float)(fabs(centre - edge) / sigma);
  return true;
}


// ---------------------------------------------------------------------------
// THE ARCHAEOLOGY. The operator, 2026-09-02:
//
//   "The sketch should take an archaeological approach. It is restoring a
//    total picture from parts. It needs however to make enough of it fit the
//    model to be confident in what it has found."
//
// A stitched passage is two fragments of one arc: the arrival, observed on the
// way in, and the departure, observed on the way out, with the stop excised
// between them. This restores the arc from the fragments -- and refuses to
// pretend the excised portion was seen.
//
// WHAT WAS MEASURED, and what each rule rests on (docs/NAVI_ONE_TWO_SIDED_
// CALIBRATION_20260902.md, 312 real accepted passages):
//
//   * A fragment can set the SCALE only if it contains the apex. Reaching 45%
//     of peak its free-fitted amplitude is wrong by 45% (median); at 75% still
//     10% median / 52% p95; only at 97-100% coverage does it settle to 2-4%
//     median, <= 10% p95. Everything short of the top is a TAIL.
//   * Given the scale, a tail either fits or it does not: fitted with the
//     trunk's amplitude fixed, real tails sit at 0.008 median, 0.018 max,
//     against the 0.13 ceiling -- at every speed ratio from 1x to 4x.
//   * A real tail's centre is never inside its own segment: it lands 21 to 53
//     samples beyond the join, >= 1.23 of its own sigma, on all 312.
//   * Two fragments that BOTH contain the apex agree on amplitude to 2.1%
//     median, 8.1% p99.
//
// OUTCOMES. Three, and the third is new:
//   Magnet        enough of one arc was seen to fit one Gaussian's scale, and
//                 every observed sample fits it.
//   Insufficient  the fragments may well be a magnet, but not enough survived
//                 to say -- typically no fragment reached the apex. This is NOT
//                 wrong shape. It is "a marker may have gone uncounted", which
//                 the firmware already knows how to say.
//   WrongShape    the fragments contradict one arc: two apexes, a trunk that
//                 is not a Gaussian, a tail that does not fit the trunk.
//
// Nothing here loosens the recognizer. The same ceiling is applied to every
// fragment, the amplitude floor and guard are untouched upstream, and a record
// with no join goes nowhere near this.
// ---------------------------------------------------------------------------
enum class Arch : uint8_t { Magnet = 0, Insufficient, WrongShape };

inline const char* archName(Arch a) {
  switch (a) {
    case Arch::Magnet:       return "MAGNET";
    case Arch::Insufficient: return "INSUFFICIENT";
    case Arch::WrongShape:   return "WRONG_SHAPE";
  }
  return "?";
}

struct ArchVerdict {
  Arch     outcome  = Arch::Insufficient;
  const char* why   = "";
  // what was found
  float    amp      = 0;      // the restored arc's amplitude
  float    sigmaArr = 0, sigmaDep = 0;
  float    residArr = 0, residDep = 0;   // each fragment against the restored arc
  uint8_t  trunk    = 0;      // 1 arrival, 2 departure, 3 apex at the join, 0 none
  // what each fragment showed, for the trace and the field
  float    decelArr = 1, decelDep = 1, regArr = 0, regDep = 0;
  float    fitArr   = 0, fitDep   = 0;   // free-fit amplitude, 0 if not a trunk candidate
  int16_t  maxArr   = 0, maxDep   = 0;
};

// Fit a flank whose amplitude is already known: only its width and centre are
// free. For each candidate width the centre follows in closed form from every
// sample -- a rising flank puts the apex AHEAD of each sample, a falling one
// BEHIND -- and the median of those is the centre. The residual against the
// resulting Gaussian is the test. Cheap: ~80 widths, no matrix.
struct TailFit { bool ok = false; float sigma = 0, centre = 0, residual = 1.0f; };
inline TailFit fitTailFixedAmp(const int16_t* y, uint16_t from, uint16_t to,
                               float A, bool rising, float floorFrac, int16_t peak) {
  TailFit best;
  const float thr = floorFrac * (float)peak;
  uint16_t used = 0;
  for (uint16_t i = from; i < to; ++i) if (y[i] > thr && y[i] > 1 && (float)y[i] < A) ++used;
  if (used < 5) return best;
  float cs[512];
  for (float s = 6.0f; s <= 600.0f; s *= 1.06f) {
    uint16_t n = 0;
    for (uint16_t i = from; i < to && n < 512; ++i) {
      if (!(y[i] > thr && y[i] > 1 && (float)y[i] < A)) continue;
      const float r = sqrtf(-2.0f * s * s * logf((float)y[i] / A));
      cs[n++] = rising ? (float)i + r : (float)i - r;
    }
    // median by partial sort
    for (uint16_t a = 1; a < n; ++a) { float v = cs[a]; int b = a - 1; while (b >= 0 && cs[b] > v) { cs[b + 1] = cs[b]; --b; } cs[b + 1] = v; }
    const float c = cs[n / 2];
    double se = 0; uint16_t m = 0;
    for (uint16_t i = from; i < to; ++i) {
      if (y[i] <= thr || y[i] <= 1) continue;
      const float dx = ((float)i - c) / s;
      const float e = (float)y[i] - A * expf(-0.5f * dx * dx);
      se += (double)e * e; ++m;
    }
    const float res = (float)(sqrt(se / (double)m) / A);
    if (!best.ok || res < best.residual) { best.ok = true; best.residual = res; best.sigma = s; best.centre = c; }
  }
  return best;
}

// WHAT A FRAGMENT MUST SHOW. Two numbers, measured on the sensor's own samples
// (docs/NAVI_ONE_TWO_SIDED_CALIBRATION_20260902.md, 312 real flanks against
// the four non-magnets of gate 12 E):
//
//   DECELERATION  the slope over the last fifth of the fragment as a fraction
//                 of its steepest slope. A Gaussian flank into its apex goes to
//                 zero: real flanks through the apex 0.32 median, 0.56 p95. A
//                 ramp never does: the electrical step 0.94 and 0.93, a pure
//                 ramp 0.97. A flank cut at 85% is still climbing, 0.75. So a
//                 fragment can set the SCALE only when it decelerates -- it has
//                 seen the top -- and a ramp can never be mistaken for one.
//   REGULARITY    of the smoothed increments, the fraction that go the way
//                 the flank should. Real flanks never below 0.86. A shoulder
//                 0.58, a double lobe 0.69 and 0.76: they turn back on
//                 themselves, and no magnet fragment does.
struct FragSig { float decel = 1.0f, regular = 0.0f; uint16_t n = 0, at = 0; };
// Judged AROUND THE FRAGMENT'S OWN MAXIMUM: a fragment that came over the top
// rises to it and falls from it, and both of those are regular. Regularity is
// the fraction of smoothed increments consistent with ONE rise-then-fall about
// that maximum. Deceleration is measured into the maximum from whichever side
// has room: a Gaussian flattens into its apex from both sides, a ramp from
// neither, and a flank cut short has its maximum at its edge, still climbing.
inline FragSig fragmentSignature(const int16_t* y, uint16_t from, uint16_t to) {
  FragSig s; s.n = (uint16_t)(to - from);
  if (s.n < 12) return s;
  static float m[512];                       // 5-point mean; Hall task only
  // A firmware record never exceeds 512 stored samples, but a host gate can
  // hand over more. STRIDE, never truncate: judging the first 512 of a 900-
  // sample fragment once judged a shoulder's first lobe alone and called it
  // regular (gate 13).
  const uint16_t stride = (uint16_t)((s.n + 511) / 512);
  uint16_t nm = 0;
  for (uint16_t i = (uint16_t)(from + 2); i + 2 < to && nm < 512; i = (uint16_t)(i + stride))
    m[nm++] = (float)(y[i-2] + y[i-1] + y[i] + y[i+1] + y[i+2]) / 5.0f;
  if (nm < 8) return s;
  // THE TOP OF A MAGNET IS BROAD; A SPIKE IS NARROW. Locate the maximum with a
  // window a twentieth of the fragment wide, so a stored outlier stretched to
  // eight samples by a replay's interpolation -- or a burst of them in the
  // field -- cannot stand in for the apex and turn the real rise after it
  // into "the wrong way". Finding 11's arrival carried one at 209.
  const uint16_t W = nm / 20 > 5 ? nm / 20 : 5;
  uint16_t at = 0; float bestW = -1e9f, firstW = 0, lastW = 0;
  for (uint16_t i = 0; i + W <= nm; ++i) {
    float acc = 0; for (uint16_t k = 0; k < W; ++k) acc += m[i + k];
    if (i == 0) firstW = acc; lastW = acc;
    if (acc > bestW) { bestW = acc; at = (uint16_t)(i + W / 2); }
  }
  // A MAXIMUM WITHIN NOISE OF AN EDGE IS THE EDGE. A flank cut short has its
  // top at one end; a few counts of wobble a little way in must not move it
  // inboard and make the first windows "the wrong way". Five counts over a
  // window is the noise this sensor leaves after averaging.
  const float tol = 5.0f * (float)W;
  if (firstW >= bestW - tol) at = 0;
  else if (lastW >= bestW - tol) at = (uint16_t)(nm - 1);
  s.at = (uint16_t)(from + 2 + at * stride);
  // REGULARITY AT THE ARC'S OWN SCALE. Per-sample increments on a slow crawl
  // are smaller than the noise -- finding 11's 37-second approach had a third
  // of them pointing the wrong way on a rise that never faltered. So the
  // question is asked over the same window that found the top: does the
  // field, W samples on, sit higher before the maximum and lower after it?
  // Windows straddling the maximum are not asked; the top is flat.
  // A WINDOW THAT DOES NOT MOVE SAYS NOTHING. A dwell's plateau that the
  // capture failed to excise, or the flat foot of a flank, is noise about a
  // level, and half of it will point the wrong way whatever the arc did.
  // Only windows that move by more than the noise are asked which way.
  uint16_t ok = 0, nd = 0;
  for (uint16_t k = 0; k + W < nm; ++k) {
    const int mid = (int)k + (int)W / 2;
    if (mid > (int)at - (int)W && mid < (int)at + (int)W) continue;
    const float d = m[k + W] - m[k];
    if (d > -5.0f && d < 5.0f) continue;
    ++nd;
    if ((mid < (int)at && d >= 0) || (mid > (int)at && d <= 0)) ++ok;
  }
  s.regular = nd >= 4 ? (float)ok / nd : 1.0f;
  // deceleration into the maximum, from each side that has room
  auto side = [&](uint16_t lo, uint16_t hi, bool rising) -> float {   // increments over [lo,hi)
    const uint16_t cnt = (uint16_t)(hi - lo);
    if (cnt < 8) return -1.0f;
    static float d[512]; uint16_t nd2 = 0;
    for (uint16_t i = (uint16_t)(lo + 1); i < hi && nd2 < 512; ++i) d[nd2++] = rising ? m[i] - m[i-1] : m[i-1] - m[i];
    const uint16_t w = nd2 / 10 > 3 ? nd2 / 10 : 3;
    float best = 0;
    for (uint16_t i = 0; i + w <= nd2; ++i) { float acc = 0; for (uint16_t k = 0; k < w; ++k) acc += d[i + k]; if (acc / w > best) best = acc / w; }
    const uint16_t q = nd2 / 5 > 3 ? nd2 / 5 : 3;
    float last = 0;
    if (rising) for (uint16_t i = nd2 - q; i < nd2; ++i) last += d[i];   // the end is the top
    else        for (uint16_t i = 0; i < q; ++i) last += d[i];           // the start is the top
    last /= q;
    return best > 0 ? last / best : 1.0f;
  };
  const float dr = side(0, (uint16_t)(at + 1), true);        // rising into the max
  const float df = side(at, nm, false);                       // falling out of it
  if (dr < 0 && df < 0) { s.decel = 1.0f; return s; }
  s.decel = (dr < 0) ? df : (df < 0) ? dr : (dr < df ? dr : df);
  return s;
}

#ifndef TWOSIDED_TRUNK_DECEL
#define TWOSIDED_TRUNK_DECEL 0.60f
#endif
#ifndef TWOSIDED_TRUNK_AGREE
#define TWOSIDED_TRUNK_AGREE 0.10f
#endif
static constexpr float kTrunkDecel   = TWOSIDED_TRUNK_DECEL;
static constexpr float kTrunkDecelSoft = 0.75f;   // when the other fragment corroborates it holds the top
static constexpr float kTrunkDecelRest = 0.80f;   // when the rest itself is the top: flanks cut 3 mm short run 0.74-0.77, ramps 0.93+
static constexpr int   cfg_settleSpanHalf = 10;   // settleSpan/2, the capture's band; the judgement has no config handle
static constexpr float kTrunkAgree   = TWOSIDED_TRUNK_AGREE;   // a trunk's fit vs its own observed maximum   // real through-apex p95 0.56; ramps 0.93+
static constexpr float kRegularFloor = 0.85f;   // real min 0.86; two-lobed 0.58-0.76
static constexpr float kTwoTrunkAmp  = 0.10f;   // two apex-holding halves agree to 8.1% p99

inline ArchVerdict examineInterrupted(const Passage& p, float ceiling) {
  ArchVerdict v;
  const int16_t* y = p.judged ? p.judged : p.oriented;
  const uint16_t pre = p.preSamples, join = p.stitchAt, n = p.sampleCount;
  if (!y || !join || join <= pre + 4 || n < join) { v.why = "fragment too short"; return v; }
  const int16_t peak = (int16_t)p.peakCounts;
  int16_t mA = 0, mD = 0; uint16_t atA = pre, atD = join;
  for (uint16_t i = pre; i < join; ++i) if (y[i] > mA) { mA = y[i]; atA = i; }
  for (uint16_t i = join; i < n; ++i)  if (y[i] > mD) { mD = y[i]; atD = i; }
  v.maxArr = mA; v.maxDep = mD;

  // A FRAGMENT BELOW THE FIT FLOOR CARRIES NO SHAPE EVIDENCE -- the same 20%
  // of peak the one-Gaussian fit has always ignored. It can neither establish
  // a magnet nor veto one. Two ways that happens and both are ordinary: the
  // passage paused AFTER the whole arc was crossed, resting in the fringe,
  // and what came after the join is the fringe settling to baseline; or it
  // opened on the foot just before the stop and the departure is the arc.
  // Then the OTHER fragment is judged alone, and it has to be a complete arc
  // by itself -- over the top and down the far side -- to be a find. Gate 12
  // I, X9: a full magnet crossing was refused because the fringe behind it
  // "turned back on itself". A fringe is not a fragment.
  const int16_t M0 = mA > mD ? mA : mD;
  // ...or less than a band-span of RANGE: a fringe wobbling by a dozen counts
  // has no shape to be regular about, and finding 13's arrival is one.
  int16_t loA = mA, loD = mD;
  for (uint16_t i = pre; i < join; ++i) if (y[i] < loA) loA = y[i];
  for (uint16_t i = join; i < n; ++i)  if (y[i] < loD) loD = y[i];
  const bool aVoid = (n - join < 12) || (float)mD < 0.20f * (float)M0 || (mD - loD) < 3 * cfg_settleSpanHalf;
  const bool dVoid = (join - pre < 12) || (float)mA < 0.20f * (float)M0 || (mA - loA) < 3 * cfg_settleSpanHalf;
  if (aVoid && dVoid) { v.why = "neither fragment left the baseline"; return v; }
  if (aVoid || dVoid) {
    const uint16_t f0 = aVoid ? pre : join, f1 = aVoid ? join : n;
    const int16_t  m  = aVoid ? mA : mD;
    const uint16_t at = aVoid ? atA : atD;
    const FragSig sg = fragmentSignature(y, f0, f1);
    if (aVoid) { v.decelArr = sg.decel; v.regArr = sg.regular; } else { v.decelDep = sg.decel; v.regDep = sg.regular; }
    if (sg.n < 12) { v.why = "the only fragment is too short"; return v; }
    if (sg.regular < kRegularFloor) { v.outcome = Arch::WrongShape; v.why = aVoid ? "arrival turns back on itself" : "departure turns back on itself"; return v; }
    // complete on its own: the top is inside it, and both ends are down the
    // flanks -- at or below half of it.
    const bool topInside = (at > f0 + 6) && (at + 6 < f1);
    const bool endsDown  = (float)y[f0] <= 0.5f * (float)m && (float)y[f1 - 1] <= 0.5f * (float)m;
    if (!(topInside && endsDown && sg.decel <= kTrunkDecel)) { v.why = aVoid ? "only the arrival survived, and it is not a whole arc" : "only the departure survived, and it is not a whole arc"; return v; }
    v.amp = (float)m; v.trunk = aVoid ? 1 : 2; v.outcome = Arch::Magnet;
    v.why = aVoid ? "one arc, complete before the stop" : "one arc, complete after the stop";
    return v;
  }

  // 1. WHAT DOES EACH FRAGMENT SHOW? This is the whole of the trust test, and
  //    it is a test of PATTERN, not of fit. A fragment earns the right to set
  //    the scale by being regular -- no reversal -- and by decelerating into
  //    a top. A ramp is regular and never decelerates. A second lobe reverses.
  //    A flank cut short is regular and still climbing. The Gaussian residual
  //    is deliberately NOT asked of a trunk: it measures constant speed, and a
  //    locomotive crawling into a station decelerates within the fragment.
  //    Finding 11's arrival is exactly that, and it is a magnet.
  const FragSig sa = fragmentSignature(y, pre, join);
  const FragSig sd = fragmentSignature(y, join, n);
  v.decelArr = sa.decel; v.decelDep = sd.decel; v.regArr = sa.regular; v.regDep = sd.regular;
  if (sa.n >= 12 && sa.regular < kRegularFloor) { v.outcome = Arch::WrongShape; v.why = "arrival turns back on itself"; return v; }
  if (sd.n >= 12 && sd.regular < kRegularFloor) { v.outcome = Arch::WrongShape; v.why = "departure turns back on itself"; return v; }
  // The fragment that observed the HIGHER maximum is corroborated by the
  // other: if it is 10% above, the other did not see the top. It may then
  // qualify at a softer deceleration -- real through-apex fragments run to
  // 0.76 at the very worst (97% coverage p95 0.68), the ramps sit at 0.93 and
  // above, and the gap holds. Finding 11's arrival, parked ON MM59, reached
  // 205 against a departure of 152 and decelerated at 0.67: it is the top.
  const bool aHigher = (float)mA >= 1.10f * (float)mD, dHigher = (float)mD >= 1.10f * (float)mA;
  bool trunkA = sa.n >= 12 && sa.decel <= (aHigher ? kTrunkDecelSoft : kTrunkDecel);
  bool trunkD = sd.n >= 12 && sd.decel <= (dHigher ? kTrunkDecelSoft : kTrunkDecel);
  // and a fragment that saw markedly LESS than the other cannot be the trunk
  // on its own: whatever it decelerated into, the top was elsewhere.
  if (trunkD && !trunkA && aHigher) { v.why = "the departure decelerated below the arrival's top"; return v; }
  if (trunkA && !trunkD && dHigher) { v.why = "the arrival decelerated below the departure's top"; return v; }

  // A FRAGMENT THAT DECELERATED BY NOISE gives itself away: its free fit
  // extrapolates well ABOVE what it observed, because the real top is higher.
  // A true trunk's fit lands within 5.5% of its maximum (p95); a crawl of
  // varying speed lands BELOW it, and is left alone -- the rule is one-sided
  // on purpose. Finding 11's arrival fits at 135 against a 176 maximum and is
  // a magnet; a 60%-cut flank fits at 1.4x its maximum and is not the top.
  if (trunkA) { SideFit f; if (fitHalfGaussian(y, pre, join, 0.20f, peak, f) && f.amp > 1.10f * (float)mA) trunkA = false; v.fitArr = f.ok ? f.amp : 0; }
  if (trunkD) { SideFit f; if (fitHalfGaussian(y, join, n, 0.20f, peak, f) && f.amp > 1.10f * (float)mD) trunkD = false; v.fitDep = f.ok ? f.amp : 0; }

  // NO PROMOTION FOR A MAXIMUM AT THE JOIN. A falling flank cut anywhere has
  // its maximum at its start, a rising one at its end; a rule that reads that
  // as "saw the top" makes every flank a trunk, and measured on 2026-09-02 it
  // accepted 312 of 312 records whose apex had been cut away at 60% -- and
  // the electrical step. If the top was cut away, the honest word is
  // insufficient, and the remedy is for the capture to retain it.
  const int16_t M = mA > mD ? mA : mD;
  // THE STOP WAS ON THE TOP. Neither fragment holds a top inside it -- the
  // arrival's maximum is its end, the departure's its start -- and both of
  // those edges sit within the band of the level the locomotive RESTED at,
  // which is at or above everything either fragment saw. The rest is the
  // apex: a 400 ms measurement at a known-stationary point, better than any
  // single sample in the record. What keeps a ramp-plateau-ramp out is the
  // pattern: at least one flank must have been decelerating into it (real
  // stops 3 mm from an apex: 0.74 to 0.77; the electrical step: 0.94).
  if (!trunkA && !trunkD && p.restLevel > 0) {
    const int32_t band = (int32_t)(cfg_settleSpanHalf);
    const bool aEdge = (join - 1 - atA) <= 6 && (int32_t)p.restLevel - (int32_t)y[join - 1] <= band + band;
    const bool dEdge = (atD - join) <= 6       && (int32_t)p.restLevel - (int32_t)y[join]     <= band + band;
    const bool restIsTop = (float)p.restLevel >= 0.95f * (float)M;
    const bool flattening = (sa.n >= 12 && sa.decel <= kTrunkDecelRest) || (sd.n >= 12 && sd.decel <= kTrunkDecelRest);
    if (aEdge && dEdge && restIsTop && flattening) {
      v.amp = (float)p.restLevel; v.trunk = 3; v.outcome = Arch::Magnet;
      v.why = "one arc, the stop was on the top"; return v;
    }
  }
  if (!trunkA && !trunkD) { v.why = "no fragment reached the apex"; return v; }

  // 2. ONE APEX. Two trunks whose maxima both sit well inside their own
  //    fragment are two lobes, whatever else they agree on.
  // A fragment that came over the top and back DOWN before the join has its
  // maximum inside itself and its join-side edge well below it. One such
  // fragment is a locomotive that stopped on the far flank. Two are two lobes.
  // Read at the edges, not as a fraction of length: a lobe centred in a
  // fragment can sit exactly at a length ratio, and one did.
  if (trunkA && trunkD) {
    const bool aDeep = (float)y[join - 1] < 0.85f * (float)mA && (join - 1 - atA) > 6;
    const bool dDeep = (float)y[join]     < 0.85f * (float)mD && (atD - join) > 6;
    if (aDeep && dDeep) { v.outcome = Arch::WrongShape; v.why = "two apexes"; return v; }
  }

  // 3. THE SCALE is what was OBSERVED at the top. Two trunks must agree on it
  //    (two halves through the apex: 2.1% median, 8.1% p99); when they do not,
  //    try the other reading before calling it a contradiction -- the lower
  //    one becomes a tail that must fit the higher.
  if (trunkA && trunkD) {
    if (fabsf((float)mA - (float)mD) / (float)M <= kTwoTrunkAmp) {
      v.amp = (float)M; v.trunk = 3; v.outcome = Arch::Magnet; v.why = "one arc, both halves over the top"; return v;
    }
    if (mA >= mD) trunkD = false; else trunkA = false;
    v.why = "competing trunk demoted";
  }
  v.amp = trunkA ? (float)mA : (float)mD; v.trunk = trunkA ? 1 : 2;

  // 4. THE TAIL MUST FIT THAT SCALE. Fitted with the amplitude FIXED, only its
  //    own width and centre free: real tails sit at 0.008 median, 0.018 max
  //    against the ceiling, at every speed ratio. Its apex must lie beyond
  //    itself on the trunk's side -- real tails: never inside, >= 1.2 sigma
  //    beyond; a tenth of a sigma is noise room.
  const bool demoted = (v.why[0] == 'c');
  if (!trunkA) {
    TailFit t = fitTailFixedAmp(y, pre, join, v.amp, true, 0.20f, peak);
    if (!t.ok) { v.why = "arrival too short to place"; return v; }
    v.sigmaArr = t.sigma; v.residArr = t.residual;
    if (t.residual > ceiling) { v.outcome = Arch::WrongShape; v.why = demoted ? "the two halves disagree on the magnet's size" : "arrival does not fit the trunk's scale"; return v; }
    if (t.centre < (float)(join - 1) - 0.1f * t.sigma) { v.outcome = Arch::WrongShape; v.why = "arrival's apex lies inside itself"; return v; }
  } else {
    TailFit t = fitTailFixedAmp(y, join, n, v.amp, false, 0.20f, peak);
    if (!t.ok) { v.why = "departure too short to place"; return v; }
    v.sigmaDep = t.sigma; v.residDep = t.residual;
    if (t.residual > ceiling) { v.outcome = Arch::WrongShape; v.why = demoted ? "the two halves disagree on the magnet's size" : "departure does not fit the trunk's scale"; return v; }
    if (t.centre > (float)join + 0.1f * t.sigma) { v.outcome = Arch::WrongShape; v.why = "departure's apex lies inside itself"; return v; }
  }
  v.outcome = Arch::Magnet; v.why = "one arc, restored";
  return v;
}

}  // namespace navi_one
