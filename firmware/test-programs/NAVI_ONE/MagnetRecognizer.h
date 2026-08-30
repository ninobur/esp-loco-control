#pragma once
// ---------------------------------------------------------------------------
// MagnetRecognizer — POSITION-FREE. Answers one question:
//
//        "Is this waveform a magnet passage, and is it a NEW one?"
//
// It does not know where the locomotive is, which marker is expected, or which
// way it is travelling. That separation is deliberate (adopted from CODEX's
// NAVI_FRESH): a corrupted position cannot corrupt a shape judgement it never
// sees. Identity — WHICH magnet — belongs to Navigator, and needs the map.
//
// THREE TESTS, each catching a population the others structurally cannot.
// Thresholds sit at the midpoint of the measured gap between populations on
// the 2026-08-28 circuit survey (187 real passages, 154 non-primaries):
//
//   AMPLITUDE   real 0.403..2.677   non-primary 0.081..0.280   gap -> 0.34
//               Catches weak artifacts. Rebounds live entirely here.
//               RELATIVE, never a fixed count: peak against a trailing median
//               of accepted peaks. A fixed floor of 140 would refuse MM012
//               thirty times a lap (decision 0052), and amplitude is
//               speed-independent -- confirmed 2026-08-29 across PWM 30..120,
//               median peak flat at 192..214.
//
//   SHAPE       real 0.0473..0.0805 non-primary 0.1948..1.1660  gap -> 0.13
//               Normalised RMS residual of a Gaussian fit. Catches mis-shaped
//               artifacts and partial reads. Zero overlap on the survey.
//               NOTE: NAVI_CL2's proposed ceiling of 0.031 would refuse 100%
//               of real magnets. This is why the constant is measured, not
//               chosen.
//
//   TIME        close-of-previous-accepted to open-of-this >= 200 ms.
//               Measured populations on the survey: rebounds 1..64 ms (n=152),
//               real magnets 436 ms minimum then 836 ms. 200 sits between, at
//               3.1x the largest rebound and 2.2x below the smallest real
//               passage. The operator's original 500 ms was 8x above the
//               rebound population and refused one genuine magnet.
//               Catches THE SAME MAGNET COUNTED TWICE. A re-read is full
//               amplitude and perfect shape, so neither test above can see it;
//               only elapsed time can. Note that AMPLITUDE alone refuses all
//               153 surveyed rebounds, so this test is not carrying them --
//               its unique population is the re-read, which needs only enough
//               time to have physically left the magnet.
//               The converse also holds and is why both exist: one rebound in
//               the survey arrived 222 SECONDS after the previous close,
//               following a station dwell. No time guard can reach that. It
//               was refused on amplitude.
//               Measured close-to-open, never from a motion clock and never
//               from throttle (decision 0057): a locomotive stopped with the
//               sensor in the field leaves the passage OPEN, so no guard
//               window is running and no rebound can be admitted during the
//               dwell. There is no PWM value that means "stopped" -- 14 PWM
//               moves Toby downhill at Westpoint, 35 may not move him uphill.
//
// A test WITHOUT EVIDENCE ABSTAINS and never votes in favour. The conjunction
// is one-strike: every test that ran must pass.
// ---------------------------------------------------------------------------
#include <math.h>
#include <stdint.h>

namespace navi_one {

enum class Outcome : uint8_t {
  Magnet = 0,
  TooSoon,        // inside the 500 ms guard -- rebound or re-read
  TooWeak,        // amplitude ratio below floor
  WrongShape,     // Gaussian residual above ceiling
  NoCurve,        // clean waveform expected but unusable
};

inline const char* outcomeName(Outcome o) {
  switch (o) {
    case Outcome::Magnet:     return "MAGNET";
    case Outcome::TooSoon:    return "TOO_SOON";
    case Outcome::TooWeak:    return "TOO_WEAK";
    case Outcome::WrongShape: return "WRONG_SHAPE";
    default:                  return "NO_CURVE";
  }
}

struct RecognizerConfig {
  uint32_t guardMs          = 200;      // close-to-open
  float    amplitudeFloor   = 0.34f;    // of trailing median accepted peak
  float    residualCeiling  = 0.13f;    // normalised RMS of Gaussian fit
  uint16_t bootstrapGain    = 190;      // until the median has 8 samples
};

// One completed Hall passage. Samples are ORIENTED: the passage's own polarity
// is made positive, so the recognizer never sees N or S. Polarity is reported
// separately and is the Navigator's business.
struct Passage {
  uint32_t      openedAtMs   = 0;
  uint32_t      closedAtMs   = 0;
  uint16_t      peakCounts   = 0;
  uint8_t       polarity     = 0;       // 1 = N, 0 = S -- passed through, not tested
  const int16_t* oriented    = nullptr;
  uint16_t      sampleCount  = 0;
  uint16_t      preSamples   = 0;
  bool          truncated    = false;
  bool          clipped      = false;
};

struct Verdict {
  Outcome  outcome        = Outcome::NoCurve;
  bool     isMagnet       = false;
  float    amplitudeRatio = 0.0f;
  float    residual       = 0.0f;
  bool     shapeTested    = false;      // false = abstained
  bool     guardTested    = false;      // false = abstained (no previous accept)
  uint32_t gapMs          = 0;
  uint16_t gain           = 0;
};

class MagnetRecognizer {
 public:
  explicit MagnetRecognizer(const RecognizerConfig& cfg) : cfg_(cfg) {}

  // Called on a declaration or a direction change: the gain history and the
  // guard anchor describe a frame that no longer applies.
  void reset() { gainLen_ = 0; gainHead_ = 0; haveAccepted_ = false; }

  uint16_t gain() const {
    if (gainLen_ < 8) return cfg_.bootstrapGain;
    uint16_t c[kGain];
    for (uint8_t i = 0; i < gainLen_; ++i) c[i] = gains_[i];
    for (uint8_t i = 1; i < gainLen_; ++i) {         // insertion sort, n<=31
      uint16_t v = c[i]; int j = (int)i - 1;
      while (j >= 0 && c[j] > v) { c[j + 1] = c[j]; --j; }
      c[j + 1] = v;
    }
    return c[gainLen_ / 2];
  }

  Verdict examine(const Passage& p) {
    Verdict v;
    v.gain = gain();
    v.amplitudeRatio = v.gain ? (float)p.peakCounts / (float)v.gain : 0.0f;

    // TIME. Abstains with no previous acceptance -- a declaration is truth,
    // but it is not a detected passage, so it arms no guard.
    if (haveAccepted_) {
      v.guardTested = true;
      v.gapMs = p.openedAtMs - lastAcceptedCloseMs_;
      if (v.gapMs < cfg_.guardMs) { v.outcome = Outcome::TooSoon; return v; }
    }

    // AMPLITUDE. Always has evidence.
    if (v.amplitudeRatio < cfg_.amplitudeFloor) { v.outcome = Outcome::TooWeak; return v; }

    // SHAPE. Abstains on a truncated or clipped curve -- the instrument says
    // it did not see the whole thing, and a fit to a cropped arc is not
    // evidence either way. It must not become a silent refusal.
    if (!p.truncated && !p.clipped) {
      float r;
      if (!fitResidual(p, r)) { v.outcome = Outcome::NoCurve; return v; }
      v.shapeTested = true;
      v.residual = r;
      if (r > cfg_.residualCeiling) { v.outcome = Outcome::WrongShape; return v; }
    }

    v.outcome = Outcome::Magnet;
    v.isMagnet = true;
    accept(p);
    return v;
  }

  // Normalised RMS residual of a least-squares-amplitude Gaussian fit over the
  // arc above 20% of peak. Returns false when there is no usable arc.
  static bool fitResidual(const Passage& p, float& out) {
    if (!p.oriented || p.sampleCount <= p.preSamples + 4) return false;
    const uint16_t begin = p.preSamples;
    uint16_t peakAt = begin; int16_t peak = p.oriented[begin];
    for (uint16_t i = begin + 1; i < p.sampleCount; ++i)
      if (p.oriented[i] > peak) { peak = p.oriented[i]; peakAt = i; }
    if (peak <= 0) return false;

    const float thr = 0.20f * (float)peak;
    uint16_t lo = peakAt, hi = peakAt;
    while (lo > begin && (float)p.oriented[lo - 1] > thr) --lo;
    while (hi + 1 < p.sampleCount && (float)p.oriented[hi + 1] > thr) ++hi;
    while (lo > begin && p.oriented[lo - 1] > 0) --lo;
    while (hi + 1 < p.sampleCount && p.oriented[hi + 1] > 0) ++hi;
    if (hi <= lo + 3) return false;

    float w = 0.0f, wx = 0.0f;
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = p.oriented[i] > 0 ? (float)p.oriented[i] : 0.0f;
      w += y; wx += y * (float)i;
    }
    if (w <= 0.0f) return false;
    const float centre = wx / w;

    float var = 0.0f;
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = p.oriented[i] > 0 ? (float)p.oriented[i] : 0.0f;
      float dx = (float)i - centre;
      var += y * dx * dx;
    }
    var /= w;
    if (var <= 0.0f) return false;
    const float sigma = sqrtf(var);

    float gy = 0.0f, gg = 0.0f;
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = p.oriented[i] > 0 ? (float)p.oriented[i] : 0.0f;
      float dx = ((float)i - centre) / sigma;
      float g = expf(-0.5f * dx * dx);
      gy += y * g; gg += g * g;
    }
    if (gg <= 0.0f) return false;
    const float amp = gy / gg;

    float se = 0.0f;
    const uint16_t n = (uint16_t)(hi - lo + 1);
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = p.oriented[i] > 0 ? (float)p.oriented[i] : 0.0f;
      float dx = ((float)i - centre) / sigma;
      float e = y - amp * expf(-0.5f * dx * dx);
      se += e * e;
    }
    out = sqrtf(se / (float)n) / (amp > 1.0f ? amp : 1.0f);
    return true;
  }

 private:
  static constexpr uint8_t kGain = 31;
  void accept(const Passage& p) {
    haveAccepted_ = true;
    lastAcceptedCloseMs_ = p.closedAtMs;
    gains_[gainHead_] = p.peakCounts;
    gainHead_ = (uint8_t)((gainHead_ + 1) % kGain);
    if (gainLen_ < kGain) ++gainLen_;
  }
  RecognizerConfig cfg_;
  bool     haveAccepted_ = false;
  uint32_t lastAcceptedCloseMs_ = 0;
  uint16_t gains_[kGain] = {};
  uint8_t  gainLen_ = 0, gainHead_ = 0;
};

}  // namespace navi_one
