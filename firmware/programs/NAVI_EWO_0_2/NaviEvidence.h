#pragma once

#include <stdint.h>

namespace navi_eyes {

// A Hall encounter is a preserved physical fact.  It is deliberately separate
// from the geographic conclusion NAVI may later make from it.
enum class EvidencePolarity : int8_t { Below = -1, None = 0, Above = 1 };

struct HallEncounter {
  uint32_t serial = 0;
  uint64_t timestampUs = 0;
  uint64_t irUm = 0;
  int16_t raw = 0;
  EvidencePolarity polarity = EvidencePolarity::None;
  bool irQuestionable = false;
};

}  // namespace navi_eyes
