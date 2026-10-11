#pragma once
#include "NaviEvidenceCore.h"
#include "NaviSequentialJudgment.h"

namespace ngr_evidence {
// Adapter for NAVI's existing mapped target. This does not change MM, PWM,
// STOP, IR counts or calibration. The operational core must consume its ruling.
class NaviEvidenceBridge {
 public:
  struct Ruling {
    bool newEncounter=false;
    bool expectedMagnet=false;
    bool polarityConflict=false;
    bool directionConflict=false;
    bool irDisagreement=false;
    uint8_t proposedMm=0;
    Encounter encounter{};
  };
  void resetForDeclarationOrReversal() {
    hall_.resetForDeclarationOrReversal();
    history_.clearForDeclaration();
  }
  Ruling observe(int raw,int reference,uint64_t timestampUs,uint32_t serial,
                 uint64_t irUm,int8_t observedDirection,
                 const IdentityEvidence& target, bool irCoordinateValid=false,
                 uint64_t separationUm=0, bool coordinateValid=false) {
    Ruling out;
    Encounter encounter;
    if (!hall_.observe(raw,reference,timestampUs,serial,irUm,encounter,irCoordinateValid,separationUm,coordinateValid,target.irInsideWindow))return out;
    out.newEncounter=true;
    out.encounter=encounter;
    history_.append(encounter,observedDirection);
    IdentityEvidence context=target;
    context.observedDirection=observedDirection;
    const auto judgment=judgeExpected(encounter,context);
    out.proposedMm=judgment.mm;
    out.irDisagreement=judgment.irDisagrees;
    out.expectedMagnet=judgment.ruling==IdentityRuling::ExpectedMagnet;
    out.polarityConflict=judgment.ruling==IdentityRuling::ContradictoryPolarity;
    out.directionConflict=judgment.ruling==IdentityRuling::WrongDirection;
    // Store the incumbent identity as a hypothesis only; operational NAVI
    // must explicitly commit it after its own movement/position checks.
    return out;
  }
  bool inField()const{return hall_.fieldLatched();}
  void reverse(){hall_.reverse();}
  void breakSamples(){hall_.breakSamples();}
  void markContradiction(){if(history_.size())history_.markContradiction(history_.size()-1);}
  bool recordConfirmed(size_t historyIndex,uint8_t mm) {
    return history_.identify(historyIndex,mm);
  }
  const DnaHistory& history()const{return history_;}
  template<class ExpectedPolarity>
  DnaCheck verify(ExpectedPolarity expected)const{
    return verifyIdentified(history_,expected);
  }
 private:
  HallQualifier hall_{};
  DnaHistory history_{};
};
}
