#pragma once
// ---------------------------------------------------------------------------
// MagnetRecognizer — POSITION-FREE. Answers one question:
//
//        "Is this waveform a magnet passage, and is it a NEW one?"
//
// It does not know where the locomotive is, which marker is expected, or which
// way it is travelling. Identity — WHICH magnet — belongs to Navigator and
// needs the map.
//
// MORPHOLOGY IS NOT COMPILED INTO THE PRODUCTION PATH (decisions 0080/0082).
// Gaussian analysis belongs to an optional read-only diagnostic observer. This
// recognizer admits on time and amplitude. It may compare a passage's polarity
// with the previous accepted passage only for decision 0083's post-stop timing
// exception; marker polarity and route sequence remain Navigator's authority.
//
// TWO PRODUCTION TESTS, each catching a population the other structurally
// cannot. Thresholds come from the measured 2026-08-28 circuit survey:
//
//   AMPLITUDE   real 0.403..2.677   non-primary 0.081..0.280   gap -> 0.34
//               Catches weak artifacts. Rebounds live entirely here.
//               RELATIVE, never a fixed count: peak against a trailing median
//               of accepted peaks. A fixed floor of 140 would refuse MM012
//               thirty times a lap (decision 0052), and amplitude is
//               speed-independent -- confirmed 2026-08-29 across PWM 30..120,
//               median peak flat at 192..214.
//
//   TIME        close-of-previous-accepted to open-of-this >= 500 ms.
//
//               X19 NOTE. This recognizer is UNCHANGED from X18 and is shared
//               verbatim. But X19 has no close: it presents each excursion
//               with openedAtMs == closedAtMs == the DETECTION INSTANT, so the
//               guard measures detect-to-detect. That is the same interval the
//               detector's own 500 ms refractory already enforces, so under
//               X19 this test can no longer refuse anything -- it abstains in
//               practice while still reporting gapMs. In X19 the refractory,
//               not this guard, is the duplicate-count protection. Do not read
//               a TOO_SOON in X19 telemetry as evidence that the guard is
//               working; read gapMs instead.
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
//               full amplitude, so amplitude cannot see it; only elapsed time
//               can. Note that AMPLITUDE alone refuses all
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
// A test WITHOUT EVIDENCE ABSTAINS and never votes in favour.
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
  uint16_t bootstrapGain    = 190;      // until the median has 8 samples
};

// One completed Hall passage. Samples are ORIENTED: the passage's own polarity
// is made positive, so the recognizer never sees N or S. Polarity is reported
// separately and is the Navigator's business.
struct Passage {
  uint32_t      openedAtMs   = 0;
  uint32_t      closedAtMs   = 0;
  uint16_t      peakCounts   = 0;
  uint8_t       polarity     = 0;       // 1=N, 0=S; relative comparison only in 0083
  const int16_t* oriented    = nullptr;   // THE RECORDING. Never filtered.
  // The JUDGEMENT COPY: `oriented` passed through a median of three, built
  // once at capture close. Peak and shape are read from this; the recording
  // above is what gets stored and published, spikes and all. Decision 0065.
  const int16_t* judged      = nullptr;
  uint16_t      sampleCount  = 0;
  uint16_t      preSamples   = 0;
  bool          truncated    = false;
  bool          clipped      = false;
  uint16_t      decimation   = 1;       // X18: samples per stored point. X19
                                        // never decimates and always sets 1.
  // Signed sum of every sample in the passage. Its sign IS the polarity
  // (X18 HallCapture::close; X19 sums the excursion only). Carried so a
  // judgement's basis travels with it;
  // nothing thresholds on it. Decision 0064.
  int64_t       signedSum    = 0;
  // Diagnostic provenance only: the fixed reference against which this
  // passage's samples, polarity and peak were measured.
  int32_t       entryBaseline = 0;
};

// Median of three, endpoints copied. Accepted decision 0065. The raw recording
// above remains untouched; only this judgement copy is filtered.
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

struct Verdict {
  Outcome  outcome        = Outcome::NoCurve;
  bool     isMagnet       = false;
  float    amplitudeRatio = 0.0f;
  float    residual       = 0.0f;
  bool     shapeTested    = false;      // false = abstained
  // Retained telemetry fields. The production recognizer does not compute
  // morphology, so these remain false/Magnet.
  bool     wouldShapeRefuse = false;
  Outcome  shapeOutcome   = Outcome::Magnet;
  bool     guardTested    = false;      // false = abstained (no previous accept)
  uint32_t gapMs          = 0;
  uint16_t gain           = 0;
  bool     postStopSuccessor = false;
};

class MagnetRecognizer {
 public:
  explicit MagnetRecognizer(const RecognizerConfig& cfg) : cfg_(cfg) {}

  // Called on a declaration or a direction change: the gain history and the
  // guard anchor describe a frame that no longer applies.
  void reset() {
    gainLen_ = 0; gainHead_ = 0; haveAccepted_ = false;
    postStopWaitingAnchor_ = false; postStopAnchorActive_ = false;
  }

  // Called only after a controlled stop reaches zero with valid navigation.
  // The first subsequently accepted passage is the departure anchor.
  void armPostStop() {
    postStopWaitingAnchor_ = true;
    postStopAnchorActive_ = false;
  }
  void cancelPostStop() {
    postStopWaitingAnchor_ = false;
    postStopAnchorActive_ = false;
  }

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
      if (v.gapMs >= cfg_.guardMs) {
        postStopAnchorActive_ = false;
      } else if (postStopAnchorActive_ && p.polarity != lastAcceptedPolarity_) {
        // This only bypasses TIME. Amplitude and Navigator must still agree.
        v.postStopSuccessor = true;
      } else {
        v.outcome = Outcome::TooSoon;
        return v;
      }
    }

    // AMPLITUDE. Always has evidence.
    if (v.amplitudeRatio < cfg_.amplitudeFloor) { v.outcome = Outcome::TooWeak; return v; }

    // THE PHYSICAL TESTS HAVE PASSED. From here the passage is a magnet.
    // In X18 the duration floor was HallCapture's (floorMs). X19 HAS NO
    // DURATION FLOOR -- see NAVIFieldConfig.h -- so amplitude is the only
    // physical screen left above the detector's 2-sample persistence test.
    // Polarity and sequence are the Navigator's. Nothing below may change
    // these two lines.
    v.outcome = Outcome::Magnet;
    v.isMagnet = true;
    const bool becomesPostStopAnchor = postStopWaitingAnchor_;
    const bool consumesPostStopSuccessor = v.postStopSuccessor;
    accept(p);
    if (becomesPostStopAnchor) {
      postStopWaitingAnchor_ = false;
      postStopAnchorActive_ = true;
    } else if (consumesPostStopSuccessor) {
      postStopAnchorActive_ = false;
    }

    return v;
  }

 private:
  static constexpr uint8_t kGain = 31;
  void accept(const Passage& p) {
    haveAccepted_ = true;
    lastAcceptedCloseMs_ = p.closedAtMs;
    lastAcceptedPolarity_ = p.polarity;
    gains_[gainHead_] = p.peakCounts;
    gainHead_ = (uint8_t)((gainHead_ + 1) % kGain);
    if (gainLen_ < kGain) ++gainLen_;
  }
  RecognizerConfig cfg_;
  bool     haveAccepted_ = false;
  uint32_t lastAcceptedCloseMs_ = 0;
  uint8_t  lastAcceptedPolarity_ = 0;
  bool     postStopWaitingAnchor_ = false;
  bool     postStopAnchorActive_ = false;
  uint16_t gains_[kGain] = {};
  uint8_t  gainLen_ = 0, gainHead_ = 0;
};

}  // namespace navi_one
