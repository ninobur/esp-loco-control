#pragma once
#include "RvmTypes.h"
#include "RvmCalibration.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace rvm {
// Incremental alignment on ONE expected portion of an immutable board.
// The geographic ordinal changes only through a Token. Numerical phase is
// revisable; neither phase crossing nor pruning is a commitment predicate.
class Recognizer {
 public:
  static constexpr int capacity=256;
  struct State { float cost, appearance, quality, weight, pendingQuality=0, pendingWeight=0; };
  explicit Recognizer(const Model& model):m_(model) {}
  void useModel(const Model& model) { m_=model; }
  void setBoot(uint64_t boot) { boot_=boot; }
  // Offline calibration only; runtime uses the frozen compiled value.
  void setAppearanceEvidence(float value){appearanceThreshold_=value;}
  void setAtypicalQuality(float value){qualityThreshold_=value;}
  void declare(int mm,int direction,uint64_t us) {
    direction_=direction; committed_=mm; expected_=wrapMm(mm+direction);
    ++run_; ordinal_=0; trusted_=mm; trustedOrdinal_=0; goodRun_=badRun_=0;
    lost_=stalled_=false; ready_=false; lastUs_=us; paused_=true; pending_=false;
    offset_=0; priorPwm_=0; pwmSlow_=0; quietUs_=0; priorHall_=0;
    firstSerial_=0; firstUs_=0; previousSerial_=0; observations_=0;
    target_=progress(m_.peaks[expected_]);
    const int previous=progress(m_.peaks[mm]);
    while(target_<=previous) target_+=m_.lapMm;
    center_=previous; phase_=float(previous);
    setBand(previous-100,target_+100);
    for(int i=0;i<capacity;++i) a_[i]=unreachable();
    // A declaration identifies an interval, not an exact sensor coordinate.
    for(int i=0;i<count_;++i) {
      const int x=(lo_+i)*m_.gridMm;
      if(x>=previous+30 && x<=target_-30) a_[i]={0,0,0,0};
    }
    seam_=progress(m_.seams[expected_]);
    while(seam_<=previous) seam_+=m_.lapMm;
  }
  bool observe(const Block& b,Token& out,uint64_t commitUs=0) {
    ++observations_;
    if(!run_ || lost_ || stalled_ || b.direction!=direction_) return false;
    if(b.lastUs<=lastUs_ || b.lastSerial<=previousSerial_) return false;
    const bool gap=b.gap || (previousSerial_ && b.firstSerial!=previousSerial_+1);
    previousSerial_=b.lastSerial;
    const float dt=float(b.lastUs-lastUs_)*1e-6f;
    lastUs_=b.lastUs;
    if(!b.pwm) { paused_=true;quietUs_=0;return false; }
    if(gap || dt>0.10f) { paused_=true;quietUs_=0;return false; }
    // Pauses remove elapsed stopped time, but retain the waveform and frontier.
    const float elapsed=paused_?0.020f:std::min(dt,0.05f); paused_=false;
    if(!ready_) {
      offset_=b.hall-baseline();ready_=true;priorHall_=b.hall;
      firstSerial_=b.firstSerial;firstUs_=b.firstUs;
    }
    const float y=b.hall-offset_;
    pwmSlow_+=std::min(1.f,elapsed/0.6f)*(float(b.pwm)-pwmSlow_);
    const int ramp=b.pwm>pwmSlow_+3?1:b.pwm<pwmSlow_-3?-1:0;
    const float speed=m_.speeds[(ramp+1)*52+std::min(51,int(b.pwm)/5)];
    const float expectedStep=speed*elapsed/m_.gridMm;
    // Zero travel remains admissible; the cap is computational, not a claim
    // that historical PWM speed is a physical limit. Bounds are reported.
    const int maxStep=std::min(12,std::max(3,int(std::ceil(1000*elapsed/m_.gridMm))));
    float best=inf();int winner=-1;
    for(int i=0;i<count_;++i) {
      const float x=float((lo_+i)*m_.gridMm);
      const Cell cell=at(int(x));
      const float mean=cell.mean4*0.25f;
      const float scale=std::max(1.5f,cell.scale4*0.25f);
      const float quality=loss((y-mean)/scale);
      // Soft, bounded Hall influence: gross corruption costs the same at all
      // incompatible cells; it cannot pull the board to fit a spike.
      const float emission=quality;
      State candidate=unreachable();
      for(int step=0;step<=maxStep && step<=i;++step) {
        const auto& p=a_[i-step];
        float motion=0.08f*std::min(2.f,std::fabs(float(step)-expectedStep));
        if(b.irValid) motion+=0.08f*std::min(2.f,std::fabs(float(step)-b.irSpeed*elapsed/m_.gridMm));
        const float c=p.cost+emission+motion;
        if(c<candidate.cost) {candidate=p;candidate.cost=c;}
      }
      if(std::isfinite(candidate.cost) && x>=seam_) {
        // Same observed sample, two waveform explanations. Both remain
        // evaluable regardless of DP window pruning. No movement prior is
        // allowed into this magnetic evidence contrast.
        const float noNew=loss((y-baseline())/std::max(scale,3.f));
        const float contrast=noNew-emission;
        candidate.appearance=std::max(0.f,candidate.appearance+contrast*elapsed);
        candidate.quality+=quality*elapsed;candidate.weight+=elapsed;
      }
      if(std::isfinite(candidate.cost) && pending_ && x<seam_){
        candidate.pendingQuality+=quality*elapsed;candidate.pendingWeight+=elapsed;
      }
      b_[i]=candidate;
      if(candidate.cost<best) {best=candidate.cost;winner=i;}
    }
    if(winner<0 || !std::isfinite(best)) {++unreachable_;return false;}
    for(int i=0;i<count_;++i) {a_[i]=b_[i];if(std::isfinite(a_[i].cost))a_[i].cost-=best;}
    const float oldPhase=phase_;phase_=float((lo_+winner)*m_.gridMm);
    const auto chosen=a_[winner];
    latestEvidence_=chosen.appearance;
    latestQuality_=chosen.weight>0?chosen.quality/chosen.weight:0;
    // Finalize the NEW MM's quality once its trailing context has reached the
    // next recorded BL seam. This is diagnostic assessment, not a second tile.
    // Thus a weak peak/trailing mismatch cannot disappear after an early token.
    if(pending_ && phase_>=seam_){
      const float q=chosen.pendingWeight>0?chosen.pendingQuality/chosen.pendingWeight:8.f;
      const bool bad=q>qualityThreshold_;
      if(bad){++badRun_;goodRun_=0;}else{badRun_=0;++goodRun_;}
      if(!bad && goodRun_>=2){trusted_=pendingToken_.committed;trustedOrdinal_=pendingToken_.ordinal;}
      out=pendingToken_;out.lastSerial=b.lastSerial;out.lastUs=b.lastUs;
      out.commitUs=commitUs?commitUs:b.lastUs;out.quality=q;out.atypical=bad;
      out.mismatches=uint8_t(badRun_);out.trusted=trusted_;out.finalQuality=true;
      pending_=false;if(badRun_>=5)lost_=true;
      priorHall_=b.hall;quietUs_=0;
      return true;
    }
    // Credible mapped ordinary track supports slow run-wide offset correction.
    // No per-magnet gain fitting; PWM zero and anomalous context cannot update it.
    if(!badRun_ && std::fabs(at(int(phase_)).mean4*0.25f-baseline())<2.f &&
       latestQuality_<qualityThreshold_ && phase_>oldPhase)
      offset_+=std::min(1.f,elapsed/5.f)*std::max(-10.f,std::min(10.f,y-baseline()));
    // Stall requires agreement of observed stationary Hall and fresh, internally
    // intact IR reporting no rotation under substantial commanded movement.
    // No timeout from missing Hall/IR alone and no motor command proves travel.
    if(b.pwm>=50 && speed>=60 && b.irValid && b.irSpeed<1 &&
       std::fabs(b.hall-priorHall_)<3 && std::fabs(phase_-oldPhase)<m_.gridMm)
      quietUs_+=uint64_t(elapsed*1e6f);
    else quietUs_=0;
    priorHall_=b.hall;
    if(quietUs_>=3000000) {
      stalled_=true;
      out={boot_,firstUs_,b.lastUs,commitUs?commitUs:b.lastUs,run_,ordinal_,
           firstSerial_,b.lastSerial,m_.version,expected_,committed_,trusted_,
           b.pwm,uint8_t(badRun_),b.irValid,false,chosen.appearance,latestQuality_,phase_};
      out.stopDecision=true;return true;
    }
    if(chosen.appearance<appearanceThreshold_) return false;
    // One new-quality contribution. Shared preceding MM is alignment context,
    // never a second strike. Only coherent multi-piece history promotes trust.
    const bool bad=latestQuality_>qualityThreshold_;
    committed_=expected_; ++ordinal_;
    out={boot_,firstUs_,b.lastUs,commitUs?commitUs:b.lastUs,run_,ordinal_,
         firstSerial_,b.lastSerial,m_.version,expected_,committed_,trusted_,
         b.pwm,uint8_t(badRun_),b.irValid,bad,chosen.appearance,latestQuality_,phase_};
    pendingToken_=out;pending_=true;
    firstSerial_=b.firstSerial;firstUs_=b.firstUs;
    center_=target_;expected_=wrapMm(expected_+direction_);
    target_=progress(m_.peaks[expected_]);while(target_<=center_)target_+=m_.lapMm;
    seam_=progress(m_.seams[expected_]);while(seam_<=center_)seam_+=m_.lapMm;
    // Slide bounded storage, retaining accumulated alignment costs. The board
    // and trusted phase history are never re-seeded at a questionable token.
    const int newLo=(center_-100)/m_.gridMm;
    const int newCount=std::min(capacity,(target_+100)/m_.gridMm-newLo+1);
    for(int i=0;i<newCount;++i) {
      const int old=newLo+i-lo_;
      b_[i]=(old>=0&&old<count_)?a_[old]:unreachable();
      b_[i].pendingQuality=b_[i].quality;b_[i].pendingWeight=b_[i].weight;
      b_[i].appearance=b_[i].quality=b_[i].weight=0;
    }
    lo_=newLo;count_=newCount;
    for(int i=0;i<count_;++i)a_[i]=b_[i];
    return true;
  }
  int committed()const{return committed_;} int expected()const{return expected_;}
  int trusted()const{return trusted_;} uint32_t trustedOrdinal()const{return trustedOrdinal_;}
  int mismatches()const{return badRun_;} float phase()const{return phase_;}
  float offset()const{return offset_;} float quality()const{return latestQuality_;}
  float evidence()const{return latestEvidence_;} uint32_t ordinal()const{return ordinal_;}
  bool lossOfPattern()const{return lost_;} bool stalled()const{return stalled_;}
  uint32_t unreachableCount()const{return unreachable_;}
 private:
  static float inf(){return std::numeric_limits<float>::infinity();}
  static State unreachable(){return {inf(),0,0,0};}
  static float loss(float z){const float a=std::fabs(z);return std::min(8.f,a<2?0.5f*a*a:2*a-2);}
  int wrapMm(int j)const{return (j%m_.magnets+m_.magnets)%m_.magnets;}
  int progress(int x)const{return direction_>0?x:m_.lapMm-x;}
  Cell at(int x)const {int raw=(direction_>0?x:-x)%m_.lapMm;if(raw<0)raw+=m_.lapMm;return m_.cells[std::min(m_.size-1,raw/m_.gridMm)];}
  float baseline()const{return m_.baselines4[expected_]*0.25f;}
  void setBand(int first,int last){lo_=first/m_.gridMm;count_=std::min(capacity,last/m_.gridMm-lo_+1);}
  Model m_; State a_[capacity]{},b_[capacity]{};
  int lo_=0,count_=0,center_=0,target_=0,seam_=0,direction_=1;
  int committed_=0,expected_=1,trusted_=0,badRun_=0,goodRun_=0;
  uint32_t run_=0,ordinal_=0,trustedOrdinal_=0,firstSerial_=0,previousSerial_=0,observations_=0,unreachable_=0;
  uint64_t boot_=0,lastUs_=0,firstUs_=0,quietUs_=0;
  float phase_=0,offset_=0,priorPwm_=0,pwmSlow_=0,priorHall_=0,latestEvidence_=0,latestQuality_=0;
  float appearanceThreshold_=rvm_calibration::appearanceEvidence;
  float qualityThreshold_=rvm_calibration::atypicalQuality;
  bool ready_=false,paused_=true,lost_=false,stalled_=false,pending_=false;
  Token pendingToken_{};
};
}
