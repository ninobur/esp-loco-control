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
#include "MagnetRecognizer.h"

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

}  // namespace navi_one
