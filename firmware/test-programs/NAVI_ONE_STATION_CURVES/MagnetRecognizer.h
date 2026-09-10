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
// SHAPE IS A MEASUREMENT, NOT A TEST (decision 0074, 2026-09-03). The
// Gaussian residual and the two-sided archaeology are still computed on every
// passage and travel with the verdict for the trace, the wire and the archive,
// and `wouldShapeRefuse` records what the former rule would have done. They
// may not refuse a passage. Every real magnet observed on this railway was
// identifiable by amplitude, time and polarity; the residual's only deciding
// votes were cast against real magnets whose records were cut short (Arches
// CW, seq 1805, 0.1372, ratio 1.005, correct pole: the locomotive stopped).
// What this gives up, stated plainly: a synthetic electrical step, shoulder
// or double lobe that ALSO clears the amplitude floor, the time guard and the
// expected pole is now admitted. None has been observed. The survey's 154
// non-primaries were all refused on amplitude alone.
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
//   TIME        close-of-previous-accepted to open-of-this >= 500 ms.
//
//               OPERATOR RULING 2026-09-10, restoring his original value. Read
//               the correction before changing this number again.
//
//               This block previously read 200 ms and justified it like this:
//               measured populations on the 2026-08-28 survey were rebounds
//               1..64 ms (n=152) and "real magnets 436 ms minimum then 836 ms";
//               200 sat between them; and the operator's 500 ms "refused one
//               genuine magnet". The arithmetic was right and the conclusion
//               was wrong, because the 436 ms event was never checked against
//               the speed the locomotive can physically reach.
//
//               A 436 ms close-to-open gap over the shortest map interval of
//               280 mm requires about 473 mm/s -- 88 pKph. Toby's fastest
//               marker-to-marker pass in that same survey was 319 mm/s
//               (59 pKph) and the fleet maximum is 400 mm/s (74 pKph). By
//               Toby's own PWM/speed fit, 88 pKph needs PWM 144. The highest
//               throttle anywhere in that dataset is 120, on a run driven
//               manually at cruise 90.
//
//               So the "one genuine magnet" that 500 ms refused could not have
//               been a genuine adjacent-magnet passage. It has the signature of
//               a re-read -- precisely the population this guard exists to
//               refuse. The guard was widened to admit the thing it was built
//               to catch, on a sample of one.
//
//               The rest of that population supports it: the header's own next
//               value, 836 ms, plus a ~160 ms passage gives a 996 ms traverse
//               = 281 mm/s = 52 pKph, squarely inside Toby's measured range.
//               The genuine population begins at 836 ms. 436 sits alone below
//               the physical floor with a 400 ms hole above it.
//
//               500 ms therefore clears the rebound population (max 64 ms) by
//               nearly 8x and still sits 336 ms below the smallest genuine
//               gap. On Otto it refuses both known double-reads, which measure
//               209 ms and 290 ms close-to-open, and which a 200 ms guard
//               admits.
//
//               BEFORE PROPOSING ANY VALUE HERE: convert the candidate gap to
//               mm/s, to pKph, and to the PWM the locomotive would need. If the
//               PWM exceeds anything in the dataset, the event is an artifact,
//               not a data point.
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
  TooSoon,        // inside the rebound guard (500 ms, operator) -- a re-read
  TooWeak,        // amplitude ratio below floor
  WrongShape,     // Gaussian residual above ceiling
  // A passage split by a stop, of which not enough survived to say what it
  // was: no fragment reached the apex. NOT wrong shape -- it may well have
  // been a magnet -- and not a magnet either. "A marker may have gone
  // uncounted", which the caller already knows how to say. Decision 0070's
  // archaeology, 2026-09-02.
  Insufficient,
  NoCurve,        // clean waveform expected but unusable
};

inline const char* outcomeName(Outcome o) {
  switch (o) {
    case Outcome::Magnet:     return "MAGNET";
    case Outcome::TooSoon:    return "TOO_SOON";
    case Outcome::TooWeak:    return "TOO_WEAK";
    case Outcome::WrongShape: return "WRONG_SHAPE";
    case Outcome::Insufficient: return "INSUFFICIENT";
    default:                  return "NO_CURVE";
  }
}

struct RecognizerConfig {
  uint32_t guardMs          = 500;      // close-to-open; operator ruling 2026-09-10
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
  // WHERE THE RECORD WAS JOINED. 0 means it was not: an ordinary passage is
  // one movement interval and has no boundary in it. Non-zero is the index of
  // the FIRST sample of the departure segment, so [preSamples, stitchAt) is
  // the arrival and [stitchAt, sampleCount) is the departure. Carried for
  // diagnostics and for any judgement that needs to know the passage was
  // observed in two intervals; the recording either side of it is untouched.
  uint16_t      stitchAt     = 0;
  // TRUE if a controlled stop was in progress at any point while this passage
  // was open -- an approach ramp, a dwell, or a departure. It does not say the
  // passage paused; it says the passage happened AROUND a stop. A refusal that
  // carries this loses a marker exactly where the geometry is most delicate,
  // and must not be allowed to pass quietly.
  bool          stopEpisode  = false;
  // THE LEVEL THE LOCOMOTIVE RESTED AT during the pause -- oriented, entry-
  // baseline-relative, the median of a 400 ms flat window. Zero if it never
  // paused. It is the best-measured sample in the passage, and for a stop on
  // the top of a magnet it is the apex itself: the band round it excises the
  // last few counts of both flanks, so neither fragment carries the top --
  // this does.
  int16_t       restLevel    = 0;
  // Signed sum of every sample in the passage. Its sign IS the polarity
  // (HallCapture::close). Carried so a judgement's basis travels with it;
  // nothing thresholds on it. Decision 0064.
  int64_t       signedSum    = 0;
};

// THE JUDGEMENT COPY: a median of FIVE, endpoints held. One reading cannot
// carry a value here, and from 2026-09-02 neither can two: a lone sample is
// outvoted by the two beside it, an adjacent pair by the three around them.
// The recording itself is never altered (decision 0065); this is the copy the
// peak and the shape are read from.
//
// WHY FIVE. Bamboo CCW, 2026-09-02 18:55:54, X9: an ordinary crossing of
// MM157 on the zero ramp -- 356 samples, a clean rise to 192 and a broad top
// -- carried a BURST on its falling flank, samples 272-280: 105, -2, -13, 108,
// 109, 174, 68, 48. Adjacent pairs, and a three-wide median leaves them in.
// Residual 0.1506, refused, and the stop-episode rule stopped the train for
// a burst of bad readings on a good magnet. Under a five-wide median the
// same record is 0.0816 -- an ordinary magnet -- and with the burst simply
// interpolated away it is 0.0825, so the five-wide median recovers exactly
// what the burst cost and nothing else.
//
// WHAT IT COSTS, measured on 312 real accepted passages: nothing. Residual
// p50 0.0709 under either width, the largest change 0.0009; peak moved by at
// most 4 counts (p95 3). Every waveform-replay gate passes with it: the 2799
// survey records, the 2026-08-29 lap, the survey polarity replay, gates 12
// and 13. Gate 6 part A wanted the peak to the count and now allows the 4.
//
// WHERE IT COULD BITE. The copy is built on the STORED record, so at heavy
// decimation five samples is a long time -- 640 ms at decimation 128 -- and
// a real feature that short would be smoothed. Nothing this recognizer
// accepts is that short; a departure stub of eight samples at 128 ms (Bamboo
// 12:29) was already unjudgeable for other reasons. Registered, not fixed.
inline void medianOfFive(const int16_t* in, uint16_t n, int16_t* out) {
  for (uint16_t i = 0; i < n; ++i) {
    int a = (int)i - 2; if (a < 0) a = 0;
    int b = (int)i + 2; if (b > (int)n - 1) b = (int)n - 1;
    int16_t w[5]; int m = 0;
    for (int q = a; q <= b; ++q) w[m++] = in[q];
    for (int x = 1; x < m; ++x) { int16_t v = w[x]; int y = x - 1; while (y >= 0 && w[y] > v) { w[y + 1] = w[y]; --y; } w[y + 1] = v; }
    out[i] = w[m / 2];
  }
}
// The old name, kept so nothing that reads a record needs to know which width
// the judgement uses this week.
inline void medianOfThree(const int16_t* in, uint16_t n, int16_t* out) { medianOfFive(in, n, out); }

}  // namespace navi_one
// The archaeology for a passage split by a stop. Needs Passage and
// medianOfThree above; provides examineInterrupted(). It opens the namespace
// itself, so it is included between two halves of this one.
#include "TwoSided.h"
namespace navi_one {

struct Verdict {
  Outcome  outcome        = Outcome::NoCurve;
  bool     isMagnet       = false;
  float    amplitudeRatio = 0.0f;
  float    residual       = 0.0f;
  bool     shapeTested    = false;      // false = abstained
  bool     twoSided       = false;      // judged as two fragments of one arc (stitchAt != 0)
  uint8_t  trunk          = 0;          // 1 arrival set the scale, 2 departure, 3 both
  const char* why         = "";         // the archaeology's reason, for the trace and the field
  ArchVerdict arch;                     // and everything it measured
  // DIAGNOSTIC ONLY (0074): what the shape rule would have said. Never
  // consulted for isMagnet. shapeOutcome is Magnet when shape passed or
  // abstained, else WrongShape / NoCurve / Insufficient.
  bool     wouldShapeRefuse = false;
  Outcome  shapeOutcome   = Outcome::Magnet;
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

    // THE PHYSICAL TESTS HAVE PASSED. From here the passage is a magnet.
    // The duration floor is HallCapture's (floorMs); polarity and sequence
    // are the Navigator's. Nothing below may change these two lines.
    v.outcome = Outcome::Magnet;
    v.isMagnet = true;
    accept(p);

    // SHAPE, measured and recorded, not obeyed (decision 0074). Abstains on
    // a truncated or clipped curve as before.
    if (!p.truncated && !p.clipped) {
      float r;
      if (p.stitchAt) {
        // TWO FRAGMENTS OF ONE ARC. One Gaussian with one width is the wrong
        // model for a passage observed in two movement intervals: measured on
        // 312 real magnets, the halves differ in width by 10% median even
        // uninterrupted, and a stop can put a 4x speed change between them.
        // The archaeology restores the arc from the fragments -- the one that
        // reached the apex sets the scale, the other must fit it -- under the
        // SAME ceiling, and says "insufficient" when not enough survived.
        const ArchVerdict a = examineInterrupted(p, cfg_.residualCeiling);
        v.shapeTested = true; v.twoSided = true; v.trunk = a.trunk; v.why = a.why; v.arch = a;
        v.residual = a.residArr > a.residDep ? a.residArr : a.residDep;
        if (a.outcome == Arch::Insufficient) v.shapeOutcome = Outcome::Insufficient;
        if (a.outcome == Arch::WrongShape)   v.shapeOutcome = Outcome::WrongShape;
      } else if (!fitResidual(p, r)) {
        v.shapeOutcome = Outcome::NoCurve;
      } else {
        v.shapeTested = true;
        v.residual = r;
        if (r > cfg_.residualCeiling) v.shapeOutcome = Outcome::WrongShape;
      }
    }
    v.wouldShapeRefuse = (v.shapeOutcome != Outcome::Magnet);
    return v;
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
