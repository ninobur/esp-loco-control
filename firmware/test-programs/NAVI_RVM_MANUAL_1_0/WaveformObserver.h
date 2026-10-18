#pragma once
#include "ReferenceTypes.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace manual_rvm {
struct Observation {
  uint64_t us;
  float hall;
  uint8_t pwm;
  bool gap=false;
};
struct Report {
  int mm=-1,next=-1,observedPolarity=0,expectedPolarity=0;
  float phase=0,pwmDistance=0,quality=0,evidence=0,uncertainty=0;
  uint64_t anchorUs=0;
  float anchorLow=0,anchorHigh=0;
  uint32_t anchors=0;
  bool ambiguous=true,changed=false;
};
// One incremental, monotone waveform alignment. The band moves with numerical
// phase, never with an MM event. All reachable states descend from the declared
// interval and every accepted waveform registration. There is no search reset.
class WaveformObserver {
 public:
  static constexpr int capacity=384;
  explicit WaveformObserver(const Model& cw,const Model& ccw):cw_(cw),ccw_(ccw),m_(&cw) {}
  bool declare(int mm,int direction,uint64_t us) {
    if(mm<0 || mm>=cw_.magnets || (direction!=1 && direction!=-1)) return false;
    dir_=direction; m_=dir_>0?&cw_:&ccw_; lastUs_=us; active_=true; offsetReady_=false;
    report_={}; report_.mm=mm; report_.next=wrap(mm+dir_);
    const float start=progress(m_->peaks[mm]);
    float end=progress(m_->peaks[report_.next]); while(end<=start) end+=m_->lapMm;
    lo_=int(std::floor((start-100)/m_->gridMm));
    for(int i=0;i<capacity;++i) {
      a_[i]=State{};
      const float x=position(i);
      if(x>=start && x<=end-30) a_[i].cost=0;
      a_[i].part=parts_[i]=partAt(x);
    }
    report_.phase=start; lastAccepted_=noKey; return true;
  }
  // An operator reversal reflects the existing coordinate/cost frontier into
  // the other recorded direction; it does not declare a new position.
  void reverse(int direction) {
    if(!active_ || direction==dir_ || (direction!=1 && direction!=-1)) return;
    const float baselineOld=baseline(report_.next);
    const int newLo=-(lo_+capacity-1);
    for(int i=0;i<capacity;++i) b_[i]=a_[capacity-1-i];
    dir_=direction; m_=dir_>0?&cw_:&ccw_; lo_=newLo;
    report_.phase=-report_.phase; report_.next=wrap(report_.mm+dir_);
    offset_+=baselineOld-baseline(report_.next);
    for(int i=0;i<capacity;++i) {
      a_[i]=b_[i]; a_[i].part=parts_[i]=partAt(position(i));
      a_[i].sum=a_[i].weight=a_[i].contrast=a_[i].observed=a_[i].expected=0;
      a_[i].doneKey=noKey;
    }
    lastAccepted_=noKey; report_.ambiguous=true;
  }
  const Report& report() const { return report_; }
  bool declared() const { return active_; }
  int direction() const { return dir_; }
  void observe(const Observation& o) {
    report_.changed=false;
    if(!active_ || o.us<=lastUs_) return;
    const float elapsed=float(o.us-lastUs_)*1e-6f; lastUs_=o.us;
    const float speed=o.pwm ? m_->speeds[52+std::min(51,int(o.pwm)/5)] : 0;
    report_.pwmDistance+=speed*elapsed;
    const bool gap=o.gap || elapsed>0.1f;
    if(!offsetReady_ && !gap) { offset_=o.hall-baseline(report_.next); offsetReady_=true; }
    if(!offsetReady_) return;
    const float y=o.hall-offset_;
    const float dt=std::min(elapsed,0.1f);
    const float prediction=speed*dt/m_->gridMm;
    const int maxStep=o.pwm ? std::min(20,std::max(3,int(std::ceil(1000*dt/m_->gridMm)))) : 0;
    float best=inf(); int winner=-1;
    for(int i=0;i<capacity;++i) {
      const float x=position(i); const int part=parts_[i];
      const Cell c=cellAt(x);
      const float mean=c.mean4*.25f, scale=std::max(1.5f,c.scale4*.25f);
      const float residual=gap?0:loss((y-mean)/scale);
      State candidate;
      for(int step=0;step<=maxStep && step<=i;++step) {
        const State& p=a_[i-step];
        const float score=p.cost+residual+0.08f*std::min(2.f,std::fabs(step-prediction));
        if(score<candidate.cost) { candidate=p; candidate.cost=score; }
      }
      if(std::isfinite(candidate.cost)) {
        if(candidate.part!=part) {
          candidate.doneKey=int(std::floor(x/m_->lapMm))*m_->magnets+candidate.part;
          candidate.donePart=candidate.part;
          candidate.doneEvidence=candidate.contrast;
          candidate.doneQuality=candidate.weight>0?candidate.sum/candidate.weight:8;
          candidate.doneObserved=candidate.observed; candidate.doneExpected=candidate.expected;
          candidate.part=part; candidate.sum=candidate.weight=candidate.contrast=0;
          candidate.observed=candidate.expected=0;
        }
        // No evidence is fabricated for acquisition gaps. Actual flat Hall IS
        // evidence, scored against both the waveform and its ordinary baseline.
        if(!gap) {
          candidate.sum+=residual*dt; candidate.weight+=dt;
          candidate.contrast+=(loss((y-baseline(part))/std::max(scale,3.f))-residual)*dt;
          candidate.observed+=(y-baseline(part))*dt;
          candidate.expected+=(mean-baseline(part))*dt;
        }
      }
      b_[i]=candidate;
      if(candidate.cost<best) { best=candidate.cost; winner=i; }
    }
    if(winner<0) { report_.ambiguous=true; return; }
    float low=position(winner),high=low;
    for(int i=0;i<capacity;++i) {
      a_[i]=b_[i]; a_[i].cost-=best;
      if(a_[i].cost<=1) { low=std::min(low,position(i)); high=std::max(high,position(i)); }
    }
    const State chosen=a_[winner]; report_.phase=position(winner);
    report_.uncertainty=high-low;
    report_.quality=chosen.doneQuality; report_.evidence=chosen.doneEvidence;
    report_.observedPolarity=sign(chosen.doneObserved);
    report_.expectedPolarity=sign(chosen.doneExpected);
    report_.ambiguous=gap || high-low>100 || chosen.doneEvidence<0.616107f || chosen.doneQuality>2;
    if(!report_.ambiguous && chosen.doneKey!=noKey && chosen.doneKey!=lastAccepted_) {
      lastAccepted_=chosen.doneKey;
      report_.mm=chosen.donePart; report_.next=wrap(report_.mm+dir_); report_.changed=true;
      report_.anchorUs=o.us; report_.anchorLow=low; report_.anchorHigh=high; ++report_.anchors;
      // Freeze the waveform-to-coordinate registration at this observation
      // time. Future paths must descend from this interval AND the same
      // completed waveform. No label-only "trusted MM" is consulted here.
      for(int i=0;i<capacity;++i)
        if(position(i)<low || position(i)>high || a_[i].doneKey!=chosen.doneKey)
          a_[i]=State{};
    }
    // Storage follows the numerical continuation even when nothing is matched.
    // Copy accumulated costs; newly exposed cells are unreachable, never seeds.
    const int shift=winner-capacity/3;
    if(shift>0) {
      for(int i=0;i<capacity;++i) b_[i]=i+shift<capacity?a_[i+shift]:State{};
      for(int i=0;i<capacity;++i) a_[i]=b_[i];
      lo_+=shift;
      for(int i=0;i<capacity;++i) parts_[i]=i+shift<capacity?parts_[i+shift]:partAt(position(i));
    }
  }
 private:
  static constexpr int noKey=std::numeric_limits<int>::min();
  static float inf() { return std::numeric_limits<float>::infinity(); }
  struct State {
    float cost=inf(),sum=0,weight=0,contrast=0,observed=0,expected=0;
    float doneEvidence=0,doneQuality=8,doneObserved=0,doneExpected=0;
    int part=0,donePart=0,doneKey=noKey;
  };
  static int sign(float v) { return (v>0)-(v<0); }
  static float loss(float z) { float a=std::fabs(z); return std::min(8.f,a<2?.5f*a*a:2*a-2); }
  int wrap(int mm) const { return (mm%m_->magnets+m_->magnets)%m_->magnets; }
  float progress(float world) const { return dir_>0?world:m_->lapMm-world; }
  float position(int i) const { return float((lo_+i)*m_->gridMm); }
  float periodic(float x) const { x=std::fmod(x,float(m_->lapMm)); return x<0?x+m_->lapMm:x; }
  Cell cellAt(float x) const {
    return m_->cells[std::min(m_->size-1,int(periodic(dir_>0?x:-x))/m_->gridMm)];
  }
  int partAt(float x) const {
    const float p=periodic(x); float nearest=float(m_->lapMm); int part=0;
    for(int j=0;j<m_->magnets;++j) {
      const float behind=periodic(p-progress(m_->seams[j]));
      if(behind<nearest) { nearest=behind; part=j; }
    }
    return part;
  }
  float baseline(int mm) const { return m_->baselines4[wrap(mm)]*.25f; }
  const Model& cw_; const Model& ccw_; const Model* m_;
  State a_[capacity],b_[capacity];
  int parts_[capacity]{};
  int lo_=0,dir_=1,lastAccepted_=noKey;
  bool active_=false,offsetReady_=false;
  float offset_=0;
  uint64_t lastUs_=0;
  Report report_;
};
}
