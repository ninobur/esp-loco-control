#pragma once
#include <stdint.h>
#include "NaviEvidenceCore.h"

namespace ngr_evidence {
// NAVI alone assigns the next geographic identity. This is a pure decision
// primitive: it never advances counters, actuates, or alters IR readings.
enum class IdentityRuling : uint8_t {
  ExpectedMagnet, ContradictoryPolarity, WrongDirection
};
struct IdentityEvidence {
  uint8_t expectedMm=0;
  Polarity expectedPolarity=Polarity::None;
  int8_t expectedDirection=0;
  int8_t observedDirection=0;
  bool irCredible=false;
  bool irInsideWindow=false;
};
struct IdentityDecision {
  IdentityRuling ruling=IdentityRuling::ContradictoryPolarity;
  uint8_t mm=0;
  bool irDisagrees=false;
};
inline IdentityDecision judgeExpected(const Encounter& e,const IdentityEvidence& c){
  if((c.expectedDirection!=1 && c.expectedDirection!=-1) ||
     c.expectedDirection!=c.observedDirection)
    return {IdentityRuling::WrongDirection,c.expectedMm,false};
  if(e.polarity!=c.expectedPolarity)
    return {IdentityRuling::ContradictoryPolarity,c.expectedMm,false};
  // IR disagreement is retained as evidence, not a veto. Encounter
  // qualification (70x2 and same-magnet exclusion) happens upstream.
  return {IdentityRuling::ExpectedMagnet,c.expectedMm,
          c.irCredible && !c.irInsideWindow};
}
}
