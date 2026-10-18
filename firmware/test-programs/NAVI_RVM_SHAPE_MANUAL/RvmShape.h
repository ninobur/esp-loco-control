#pragma once
// ---------------------------------------------------------------------------
// RvmShape — statistical magnetic shape recognizer with a LOCAL baseline.
// Mirror of analysis/navi_rvm_shape_20261018/scripts/rvm_local.py (the offline
// reference); tests/navi_rvm_shape/replay.cpp checks the two agree.
//
// Input: the 20 ms Hall tracing (one value per block, NaN for a block with too
// few samples) and the block's route direction. Nothing else: no PWM, no IR,
// no speed, no distance, no time between magnets.
//
// For every new block, every grid width W (8..444 blocks) scores the window that
// has just completed against two Gaussian-mixture shape classes:
//   E = a whole magnetic ensemble (principal excursion, lobes, shoulders,
//       transitions) centred in the window;
//   N = anything else (baseline, a lobe alone, half a magnet, MM-BL-MM).
// Shape = window resampled linearly to 32 points, minus the reference level,
// principal excursion scaled to +1, reversed in time when travelling CCW;
// features = 12 DCT coefficients + log(amplitude / baseline noise).
//
// REFERENCE (the local baseline). The reference for a window is the recognized
// baseline that precedes it: every block from the end of the previously
// recognized ensemble's window (or from the operator's declaration) up to the
// window's start. Level = least-squares line through all of it, evaluated at
// the window centre; noise = mean |20 ms step| over all of it. No fixed
// duration. A window must start inside that baseline, so it never references
// itself, and the baseline's start is fixed by the previous recognition, which
// used its own predecessor, back to the declared startup baseline.
//
// DECISION. The best LLR>0 window is held; a later window whose core (middle
// half) overlaps the held core and scores higher replaces it. The held window
// is accepted once the tracing has run half its own width beyond its end. On
// acceptance the next baseline starts at that window's end and windows after it
// are re-scored against the new baseline.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "RvmShapeModel.h"

namespace rvm_shape {

struct Ensemble {
  uint32_t seq = 0;          // 1, 2, 3 ... per recognizer lifetime
  double c = 0, w = 0;       // window centre (block index, may be .5) and width (blocks)
  double s = 0, e = 0;       // core = ensemble estimate [s, e]
  double decided = 0;        // block index at which it was accepted
  uint32_t blStart = 0;      // block index where its reference baseline began
  float llr = 0, amp = 0, level = 0, noise = 0;
  int8_t pol = 0;            // +1 excursion above the baseline, -1 below
};

class Recognizer {
 public:
  static constexpr uint32_t RING = 1024;          // > MAX_W + 2 + acceptance re-scan
  static_assert(RING > uint32_t(MAX_W) * 2 + 8, "ring too small");

  // Declaration / startup: the baseline starts at block index `at` (<= blocks()).
  void startBaseline(uint32_t at) {
    active_ = true; held_ = false;
    if (at > n_) at = n_;
    b_ = at; rebuildPrefix();
    cursor_ = b_;
  }
  void stop() { active_ = false; held_ = false; }
  bool active() const { return active_; }
  uint32_t blocks() const { return n_; }
  uint32_t baselineStart() const { return b_; }
  bool holding() const { return held_; }

  // Append the next 20 ms block. Returns true if one or more ensembles were accepted.
  bool push(float hall, bool ccw) {
    const uint32_t i = n_;
    h_[i % RING] = hall; ccw_[i % RING] = ccw;
    ++n_;
    if (active_) extendPrefix(i);
    bool any = false;
    // a window ending at e needs blocks through e+1
    while (active_ && n_ >= 2 && cursor_ + 2 <= n_) any |= step(cursor_);
    return any;
  }
  bool take(Ensemble& out) {
    if (outHead_ == outTail_) return false;
    out = out_[outTail_ % OUTQ]; ++outTail_; return true;
  }
  uint32_t outputDrops() const { return outDrops_; }
  uint32_t windowsScored() const { return scored_; }

 private:
  struct Acc { double n, sx, sy, sxx, sxy, dn, ds; };   // sums over [b, i)
  struct Cand { double c, w, end; float llr, amp, lvl, noise; int8_t pol; };

  float h_[RING]; bool ccw_[RING]; Acc p_[RING];
  uint32_t n_ = 0, b_ = 0, cursor_ = 0;
  bool active_ = false, held_ = false;
  Cand B_{};
  uint32_t seq_ = 0, scored_ = 0, outDrops_ = 0;
  static constexpr uint32_t OUTQ = 16;
  Ensemble out_[OUTQ]; uint32_t outHead_ = 0, outTail_ = 0;

  float hv(uint32_t i) const { return h_[i % RING]; }

  // prefix p_[i] = sums over [b, i); p_[b] = 0
  void rebuildPrefix() {
    p_[b_ % RING] = Acc{0, 0, 0, 0, 0, 0, 0};
    for (uint32_t i = b_; i < n_; ++i) extendPrefix(i);
  }
  void extendPrefix(uint32_t i) {        // p_[i+1] from p_[i] and block i
    Acc a = p_[i % RING];
    const float y = hv(i);
    if (isfinite(y)) {
      const double x = double(i) - double(b_);
      a.n += 1; a.sx += x; a.sy += y; a.sxx += x * x; a.sxy += x * y;
    }
    if (i > 0) {
      const float y0 = hv(i - 1);
      if (isfinite(y) && isfinite(y0)) { a.dn += 1; a.ds += fabs(double(y) - double(y0)); }
    }
    p_[(i + 1) % RING] = a;
  }

  // Score the window [e-W, e] (centre e-W/2). Returns false if not evaluable.
  bool score(uint32_t e, uint32_t W, Cand& out) {
    const uint32_t ws = e - W;
    if (ws < b_ + MIN_BL || e + 1 >= n_) return false;
    const Acc& a = p_[ws % RING];
    if (a.n < MIN_BL) return false;
    for (uint32_t i = ws; i <= e + 1; ++i) if (!isfinite(hv(i))) return false;
    const double c = double(e) - W / 2.0;
    const double den = a.n * a.sxx - a.sx * a.sx;
    const double slope = den > 1e-9 ? (a.n * a.sxy - a.sx * a.sy) / den : 0.0;
    const double icpt = (a.sy - slope * a.sx) / (a.n > 1 ? a.n : 1);
    const double lvl = icpt + slope * (c - double(b_));
    double noise = 1.7724538509055159 / 2.0 * a.ds / (a.dn > 1 ? a.dn : 1);
    if (noise < 0.5) noise = 0.5;
    float x[NPT];
    int jmax = 0; float amax = -1;
    for (int j = 0; j < NPT; ++j) {
      const double pos = c + (-0.5 + double(j) / (NPT - 1)) * W;
      const double lo = floor(pos); const double fr = pos - lo;
      const uint32_t li = uint32_t(lo);
      x[j] = float(hv(li) * (1.0 - fr) + hv(li + 1) * fr - lvl);
      if (fabsf(x[j]) > amax) { amax = fabsf(x[j]); jmax = j; }
    }
    const int8_t pol = x[jmax] >= 0 ? 1 : -1;
    const float A = amax > 1e-6f ? amax : 1e-6f;
    float u[NPT];
    const bool rev = ccw_[uint32_t(c) % RING];
    for (int j = 0; j < NPT; ++j) u[rev ? NPT - 1 - j : j] = pol * x[j] / A;
    float f[DIM];
    for (int k = 0; k < NDCT; ++k) { float s = 0; for (int j = 0; j < NPT; ++j) s += DCT[k][j] * u[j]; f[k] = s; }
    f[NDCT] = logf(A / float(noise));
    for (int k = 0; k < DIM; ++k) if (!isfinite(f[k])) return false;
    ++scored_;
    out.c = c; out.w = W; out.end = e;
    out.llr = mixture(COMP_E, N_E, f) - mixture(COMP_N, N_N, f);
    out.amp = A; out.lvl = float(lvl); out.noise = float(noise); out.pol = pol;
    return true;
  }
  static float mixture(const Component* cs, int nc, const float* f) {
    float ll[16]; float m = -INFINITY;
    for (int k = 0; k < nc; ++k) {
      const Component& C = cs[k];
      float dv[DIM]; for (int i = 0; i < DIM; ++i) dv[i] = f[i] - C.mu[i];
      float q = 0; int t = 0;
      for (int i = 0; i < DIM; ++i) { float z = 0; for (int j = 0; j <= i; ++j) z += C.linv[t++] * dv[j]; q += z * z; }
      ll[k] = C.k - 0.5f * q; if (ll[k] > m) m = ll[k];
    }
    float s = 0; for (int k = 0; k < nc; ++k) s += expf(ll[k] - m);
    return m + logf(s);
  }

  void accept(double decided) {
    Ensemble o;
    o.seq = ++seq_; o.c = B_.c; o.w = B_.w; o.s = B_.c - B_.w / 4; o.e = B_.c + B_.w / 4;
    o.decided = decided; o.blStart = b_; o.llr = B_.llr; o.amp = B_.amp; o.level = B_.lvl;
    o.noise = B_.noise; o.pol = B_.pol;
    if (outHead_ - outTail_ < OUTQ) { out_[outHead_ % OUTQ] = o; ++outHead_; } else ++outDrops_;
    const double end = B_.end;
    held_ = false;
    b_ = uint32_t(ceil(end)); if (b_ > n_) b_ = n_;
    rebuildPrefix();
    cursor_ = b_ + MIN_BL + SCALES[0];
  }

  // Process all windows ending at e. Returns true on acceptance.
  bool step(uint32_t e) {
    if (held_ && double(e) >= B_.end + B_.w / 2) { accept(B_.end + B_.w / 2); return true; }
    Cand cs[N_SCALES]; int nc = 0;
    for (int k = 0; k < N_SCALES; ++k) {
      const uint32_t W = SCALES[k];
      if (e < W) continue;
      Cand c;
      if (score(e, W, c) && c.llr > 0) cs[nc++] = c;
    }
    // highest LLR first at a given end (same order as the offline reference)
    for (int i = 1; i < nc; ++i) for (int j = i; j > 0 && cs[j].llr > cs[j - 1].llr; --j) { Cand t = cs[j]; cs[j] = cs[j - 1]; cs[j - 1] = t; }
    for (int i = 0; i < nc; ++i) {
      const Cand& r = cs[i];
      if (!held_) { B_ = r; held_ = true; continue; }
      const double rs = r.c - r.w / 4, re = r.c + r.w / 4;
      if (rs < B_.c + B_.w / 4 && re > B_.c - B_.w / 4 && r.llr > B_.llr) B_ = r;
    }
    cursor_ = e + 1;
    return false;
  }
};

}  // namespace rvm_shape
