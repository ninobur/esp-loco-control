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
  TooSoon,        // inside the rebound guard (200 ms, measured) -- a re-read
  TooWeak,        // amplitude ratio below floor
  WrongShape,     // Gaussian residual above ceiling
  NoCurve,        // clean waveform expected but unusable
  // INTERRUPTED PATH ONLY (decision 0070). Appended, never inserted: the
  // outcome travels to the dashboard as a uint8_t and the existing five
  // numbers are part of the wire format.
  NoEntry,        // no changing entry into the field -- a stationary offset
};

inline const char* outcomeName(Outcome o) {
  switch (o) {
    case Outcome::Magnet:     return "MAGNET";
    case Outcome::TooSoon:    return "TOO_SOON";
    case Outcome::TooWeak:    return "TOO_WEAK";
    case Outcome::WrongShape: return "WRONG_SHAPE";
    case Outcome::NoEntry:    return "NO_ENTRY";
    default:                  return "NO_CURVE";
  }
}

struct RecognizerConfig {
  uint32_t guardMs          = 200;      // close-to-open
  float    amplitudeFloor   = 0.34f;    // of trailing median accepted peak
  float    residualCeiling  = 0.13f;    // normalised RMS of Gaussian fit
  uint16_t bootstrapGain    = 190;      // until the median has 8 samples

  // ---- INTERRUPTED TRAVERSAL (decision 0070) ----------------------------
  // A passage physically cut in half by a controlled stop. Shape is
  // unavailable, so these four carry the whole burden. THEY ARE NOT NEW
  // NUMBERS: the first three are the acquisition layer's own measured
  // constants, restated here because the recognizer may not reach into
  // CaptureConfig. checkInterruptedConfig() below refuses a build in which
  // they have drifted apart.
  int16_t  entryGrowthMin   = 38;       // == CaptureConfig::entryMargin
  int16_t  contradictionMax = 25;       // == CaptureConfig::exitMargin
  uint16_t interruptMinMs   = 40;       // == CaptureConfig::floorMs
  // "Adequate multi-sample support": judged samples at or above the entry
  // margin, in the pole's own direction. Three judged samples cannot be built
  // from fewer than five raw ones, so no spike and no adjacent pair of spikes
  // can establish a magnet here. Decision 0065's rule, carried into a segment
  // that has no shape test to fall back on.
  uint8_t  interruptMinSupport = 3;
};

// One completed Hall passage. Samples are ORIENTED: the passage's own polarity
// is made positive, so the recognizer never sees N or S. Polarity is reported
// separately and is the Navigator's business.
struct Passage {
  uint32_t      openedAtMs   = 0;
  uint32_t      closedAtMs   = 0;
  uint16_t      peakCounts   = 0;
  uint8_t       polarity     = 0;       // 1 = N, 0 = S -- passed through, not tested
  const int16_t* oriented    = nullptr;   // THE RECORDING. Never filtered.
  // The JUDGEMENT COPY: `oriented` passed through a median of three, built
  // once at capture close. Peak and shape are read from this; the recording
  // above is what gets stored and published, spikes and all. Decision 0065.
  const int16_t* judged      = nullptr;
  uint16_t      sampleCount  = 0;
  uint16_t      preSamples   = 0;
  bool          truncated    = false;
  bool          clipped      = false;
  uint16_t      decimation   = 1;       // samples per stored point (see HallCapture)
  // Signed sum of every sample in the passage. Its sign IS the polarity
  // (HallCapture::close). Carried so a judgement's basis travels with it;
  // nothing thresholds on it. Decision 0064.
  int64_t       signedSum    = 0;
};

// Median of three, endpoints copied. One reading cannot carry a value here:
// a lone sample is outvoted by the two beside it, while a real feature spanning
// two or more samples survives intact. src and dst must not overlap.
//
// On 2026-08-31 a single +313 sample in the tail of MM169 -- neighbours +17 and
// +21 -- became the passage's peak (313 against a true 185, ratio 1.68 against
// a true 1.0) and put the Gaussian residual at 0.1307 against a 0.13 ceiling.
// The magnet was fine. Finding 07.
inline void medianOfThree(const int16_t* src, uint16_t n, int16_t* dst) {
  if (!src || !dst || n == 0) return;
  if (n < 3) { for (uint16_t i = 0; i < n; ++i) dst[i] = src[i]; return; }
  dst[0] = src[0];
  for (uint16_t i = 1; i + 1 < n; ++i) {
    const int16_t a = src[i - 1], b = src[i], c = src[i + 1];
    dst[i] = a < b ? (b < c ? b : (a < c ? c : a))
                   : (a < c ? a : (b < c ? c : b));
  }
  dst[n - 1] = src[n - 1];
}

// The judgement copy, evaluated in place: med3At(v, n, i) returns exactly what
// medianOfThree(v, n, dst) would have written at dst[i]. Gate 12 asserts that
// elementwise against 20,000 random vectors.
//
// It exists because the interrupted path has to judge a segment while the
// passage is still OPEN -- HallCapture's judge_ buffer is not built until
// close() and there are 512 samples of it, which is a quarter of the Hall
// task's stack. A second RING-sized buffer would cost 1 KB of RAM to say the
// same thing.
inline int16_t med3At(const int16_t* v, uint16_t n, uint16_t i) {
  if (!v || n == 0) return 0;
  if (n < 3 || i == 0 || i + 1 >= n) return v[i];
  const int16_t a = v[i - 1], b = v[i], c = v[i + 1];
  return a < b ? (b < c ? b : (a < c ? c : a))
               : (a < c ? a : (b < c ? c : b));
}

// ---------------------------------------------------------------------------
// A SEGMENT is the part of a passage during which the field was actually
// CHANGING. It is not a Passage and must never be treated as one: it never
// closed, it has no shape, and it may not enter the gain history.
//
// Two of them exist per interruption episode at most:
//   PRE-STOP    from the passage opening to the start of the settle window.
//   DEPARTURE   from the field leaving the settled level to its return, or to
//               the sensor going clear under the ordinary exit rule.
// Stationary dwell samples appear in neither, by construction -- the settle
// window is excluded from the first and the second does not begin until the
// field moves again.
//
// TWO REFERENCES, AND WHY (see decision 0070 and the harness's case R7)
//   `v` is measured against the sensor's IDLE reference -- entryBaseline_ for
//   a pre-stop segment, the live baseline for a departure segment. Amplitude,
//   polarity, support and contradiction are read from it, because the gain
//   median is calibrated in idle-relative counts and because a departure that
//   is the DECAY of a parked field reads the opposite pole if it is measured
//   from where the locomotive was parked.
//   `growthFrom` is the level the field sat at before the segment began,
//   oriented, in the same units. Growth is peak - growthFrom, so the entry
//   test asks the only question a stationary offset cannot answer: did the
//   field CHANGE after the stop?
// ---------------------------------------------------------------------------
struct Segment {
  const int16_t* v          = nullptr;  // idle-relative, UNORIENTED
  uint16_t       n          = 0;
  uint16_t       preSamples = 0;
  uint32_t       fromMs     = 0, toMs = 0;
  uint8_t        polarity   = 1;        // sign of signedSum -- decision 0064
  int64_t        signedSum  = 0;
  int16_t        growthFrom = 0;        // oriented, idle-relative
  bool           clipped    = false;
  uint16_t       decimation = 1;
  bool           departure  = false;    // false = pre-stop. Reporting only.
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
  // Interrupted path only. `interrupted` is what tells the dashboard that a
  // shapeTested:0 verdict is a physically truncated traversal rather than a
  // railed or over-long one.
  bool     interrupted    = false;
  uint16_t peak           = 0;      // of the judged segment
  int16_t  entryGrowth    = 0;      // peak minus the level it grew from
  uint16_t support        = 0;      // judged samples at or above the entry margin
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

  // -------------------------------------------------------------------------
  // THE INTERRUPTED-TRAVERSAL RULE (decision 0070). One narrowly bounded
  // second way to establish a magnet, reachable ONLY from an identified
  // controlled stop that cut a passage in half.
  //
  // Shape is not tested and cannot be: half an arc fits a Gaussian about as
  // well as half a step does. That is the same reasoning the ordinary path
  // already applies to a truncated or clipped curve (decision 0062) -- the
  // instrument did not see the whole thing, so the test abstains rather than
  // refusing. Here it abstains for the whole class, which is why the class is
  // kept as small as it is: everything else in this file still applies, and
  // every other test is not merely retained but REQUIRED, where the ordinary
  // path lets the guard abstain.
  //
  // WHAT THIS COSTS, STATED PLAINLY. Shape is the test with zero overlap
  // between the real and non-primary populations. Without it, an interrupted
  // segment is separated from a slow electrical step of the same size and
  // sign by nothing but the fact that a station stop was in progress. See
  // decision 0070's "What this cannot do".
  //
  // The map is not consulted. Station identity, expected polarity, expected
  // marker, PWM and position select the MODE, upstream, and appear nowhere in
  // this function.
  Verdict examineInterrupted(const Segment& s) {
    Verdict v;
    v.interrupted = true;
    v.shapeTested = false;                    // published as such
    v.gain = gain();

    if (!s.v || s.n < 3) { v.outcome = Outcome::NoCurve; return v; }
    // A changing stretch shorter than the passage floor is a transient, not a
    // traversal. Same constant, same reasoning, as HallCapture's floorMs.
    if (s.toMs - s.fromMs < cfg_.interruptMinMs) { v.outcome = Outcome::NoCurve; return v; }
    if (s.clipped) { v.outcome = Outcome::NoCurve; return v; }

    // TIME. Required here, not merely abstaining: a segment carries less
    // evidence than a passage, so it does not get the benefit of a test that
    // has nothing to say.
    if (!haveAccepted_) { v.outcome = Outcome::NoCurve; return v; }
    v.guardTested = true;
    v.gapMs = s.fromMs - lastAcceptedCloseMs_;
    if (v.gapMs < cfg_.guardMs) { v.outcome = Outcome::TooSoon; return v; }

    // Everything below reads the MEDIAN-OF-THREE judgement, never the
    // recording. Decision 0065.
    const int32_t sign = s.polarity ? 1 : -1;
    int32_t peak = -32768, contra = 0;
    uint16_t support = 0;
    for (uint16_t i = 0; i < s.n; ++i) {
      const int32_t m = sign * (int32_t)med3At(s.v, s.n, i);
      if (m > peak) peak = m;
      if (-m > contra) contra = -m;
      if (m >= cfg_.entryGrowthMin) ++support;
    }
    if (peak < 0) peak = 0;
    v.peak = (uint16_t)peak;
    v.support = support;
    v.entryGrowth = (int16_t)(peak - (int32_t)s.growthFrom);
    v.amplitudeRatio = v.gain ? (float)peak / (float)v.gain : 0.0f;

    // ENTRY. The one test a stationary offset cannot pass, and the reason the
    // whole rule is safe against findings 08, 09 and 10: an offset that was
    // already there when the passage opened has no growth.
    if (v.entryGrowth < cfg_.entryGrowthMin) { v.outcome = Outcome::NoEntry; return v; }
    // AMPLITUDE. Unchanged floor, unchanged trailing median.
    if (v.amplitudeRatio < cfg_.amplitudeFloor) { v.outcome = Outcome::TooWeak; return v; }
    // SUSTAINED SIGNED EVIDENCE, and no contradiction. One sample may not
    // establish entry, amplitude, polarity or occupancy.
    if (support < cfg_.interruptMinSupport) { v.outcome = Outcome::TooWeak; return v; }
    if (contra > cfg_.contradictionMax) { v.outcome = Outcome::WrongShape; return v; }

    v.outcome = Outcome::Magnet;
    v.isMagnet = true;
    acceptInterrupted(s);
    return v;
  }

  // A counted interruption arms the rebound guard exactly as a normal
  // acceptance does -- the next passage must still be 200 ms away -- and
  // DELIBERATELY DOES NOT enter the gain median. A partial or stationary peak
  // is not a measurement of this sensor's response to a whole magnet, and
  // twenty of them would walk the amplitude floor onto the wrong number.
  void acceptInterrupted(const Segment& s) {
    haveAccepted_ = true;
    lastAcceptedCloseMs_ = s.toMs;
  }

  // The interrupted constants restate the acquisition layer's. A build in
  // which they have drifted apart is judging segments by one number and
  // opening passages by another. Called from setup(); gate 12 asserts it.
  bool checkInterruptedConfig(int16_t entryMargin, int16_t exitMargin,
                              uint16_t floorMs) const {
    return cfg_.entryGrowthMin == entryMargin &&
           cfg_.contradictionMax == exitMargin &&
           cfg_.interruptMinMs == floorMs;
  }

  // Normalised RMS residual of a least-squares-amplitude Gaussian fit over the
  // arc above 20% of peak. Returns false when there is no usable arc.
  static bool fitResidual(const Passage& p, float& out) {
    // Judged, not recorded: one sample may not decide the arc, the centre, the
    // width or the error. The recording itself is never altered (decision 0065).
    const int16_t* o = p.judged ? p.judged : p.oriented;
    if (!o || p.sampleCount <= p.preSamples + 4) return false;
    const uint16_t begin = p.preSamples;
    uint16_t peakAt = begin; int16_t peak = o[begin];
    for (uint16_t i = begin + 1; i < p.sampleCount; ++i)
      if (o[i] > peak) { peak = o[i]; peakAt = i; }
    if (peak <= 0) return false;

    const float thr = 0.20f * (float)peak;
    uint16_t lo = peakAt, hi = peakAt;
    while (lo > begin && (float)o[lo - 1] > thr) --lo;
    while (hi + 1 < p.sampleCount && (float)o[hi + 1] > thr) ++hi;
    while (lo > begin && o[lo - 1] > 0) --lo;
    while (hi + 1 < p.sampleCount && o[hi + 1] > 0) ++hi;
    if (hi <= lo + 3) return false;

    float w = 0.0f, wx = 0.0f;
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = o[i] > 0 ? (float)o[i] : 0.0f;
      w += y; wx += y * (float)i;
    }
    if (w <= 0.0f) return false;
    const float centre = wx / w;

    float var = 0.0f;
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = o[i] > 0 ? (float)o[i] : 0.0f;
      float dx = (float)i - centre;
      var += y * dx * dx;
    }
    var /= w;
    if (var <= 0.0f) return false;
    const float sigma = sqrtf(var);

    float gy = 0.0f, gg = 0.0f;
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = o[i] > 0 ? (float)o[i] : 0.0f;
      float dx = ((float)i - centre) / sigma;
      float g = expf(-0.5f * dx * dx);
      gy += y * g; gg += g * g;
    }
    if (gg <= 0.0f) return false;
    const float amp = gy / gg;

    float se = 0.0f;
    const uint16_t n = (uint16_t)(hi - lo + 1);
    for (uint16_t i = lo; i <= hi; ++i) {
      float y = o[i] > 0 ? (float)o[i] : 0.0f;
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
