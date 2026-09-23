#pragma once
#include <stdio.h>
#include "MovementEvidence.h"
#include "../../NAVI_COHERENCE/IR_ARCHITECTURE_0_4/MmDistanceReference.h"

namespace ngr_nav {
// Diagnostic-only consumer. Never returns evidence to Navigator or motor code.
// Uses the same 1 s freshness / 150 ms alignment gates as the existing adapter.
class IrHealthMonitor {
 public:
  static constexpr uint64_t FRESH_US=1000000, ALIGN_US=150000;
  void pair(const uint8_t* mac,uint64_t now) {
    memcpy(mac_,mac,6);paired_=validMac(mac);
    apply(odo_.sourceChanged());reference_.invalidate();
    have_=false;used_=head_=retiredCount_=0;exhausted_=false;
    acceptAfter_=now;lastArrival_=0;deltaAvailable_=false;
    setHealth({});++revision_;
  }
  void invalidateReference() {reference_.invalidate();++revision_;}
  void queueGap(uint64_t now) {
    ++queueGaps_;acceptAfter_=now;used_=head_=0;
    fail(IrHealthFault::OrderFault,IrEpochBreak::TransportGap);
  }
  void tick(uint64_t now) {
    if(have_ && (now<lastArrival_ || now-lastArrival_>FRESH_US))
      fail(IrHealthFault::LinkStale,IrEpochBreak::LinkStale);
  }
  void receive(const uint8_t* mac,const uint8_t* bytes,size_t size,uint64_t arrival) {
    if(!paired_ || memcmp(mac_,mac,6) || arrival<=acceptAfter_)return;
    if(size!=sizeof(WireSnapshot)) {reject();return;}
    WireSnapshot w;memcpy(&w,bytes,sizeof(w));
    if(arrival>uint64_t(INT64_MAX/4) || w.capturedUs>uint64_t(INT64_MAX/4) ||
       w.magic!=0x4952 || w.version!=1 || w.type!=5 || !w.bootId ||
       !w.pitchUm || !w.calibrationId || w.distanceValidated ||
       w.opticalReason>ir_movement::TRACKING || w.completedPulses>w.observedRises ||
       w.completedPulses>UINT64_MAX/w.pitchUm || w.nominalUm!=w.completedPulses*w.pitchUm ||
       movementCrc(bytes,offsetof(WireSnapshot,crc))!=w.crc) {reject();return;}
    if(exhausted_){++rejected_;return;}
    if(have_ && arrival<lastArrival_) {++old_;return;}
    const bool newBoot=have_ && w.bootId!=last_.bootId;
    if(newBoot) {
      for(uint8_t i=0;i<retiredCount_;++i)if(w.bootId==retired_[i]){++old_;return;}
      if(retiredCount_==8) {
        exhausted_=true;++rejected_;fail(IrHealthFault::OrderFault,IrEpochBreak::TransportGap);return;
      }
      retired_[retiredCount_++]=last_.bootId;
    } else if(have_) {
      if(w.sequence==last_.sequence){++duplicates_;return;}
      if(int32_t(w.sequence-last_.sequence)<=0 || w.capturedUs<=last_.capturedUs){++old_;return;}
    }
    // Detect a gap even if the main loop drained a backlog before its watchdog.
    tick(arrival);
    const auto previous=odo_.point();
    setHealth(classifyIrInstrument(w));
    apply(odo_.ingest(w));
    deltaAvailable_=odo_.contains(previous) && w.capturedUs>previous.capturedUs;
    if(deltaAvailable_) {deltaPulses_=w.completedPulses-previous.pulses;deltaUs_=w.capturedUs-previous.capturedUs;}
    const int64_t offset=int64_t(arrival)-int64_t(w.capturedUs);
    if(!have_ || newBoot){offset_=offset;head_=used_=0;}
    else if(offset<offset_)offset_=offset;
    last_=w;lastArrival_=arrival;have_=true;++accepted_;
    history_[head_]={w.capturedUs,odo_.point()};head_=uint8_t((head_+1)%64);
    if(used_<64)++used_;
  }
  // NAVI owns the supplied identity. This is only a shadow reference to its
  // accepted Hall event, not independent proof that NAVI chose the right MM.
  bool acceptedMm(uint8_t mm,uint32_t hallMs,uint64_t hallUs,uint32_t serial,uint64_t now) {
    tick(now);
    reference_.invalidate();refEvent_=serial;refAlignmentUs_=0;++revision_;
    if(!have_ || !odo_.haveMeasurement() || hallUs>uint64_t(INT64_MAX/4))return false;
    uint64_t best=UINT64_MAX;const Entry* entry=nullptr;
    for(uint8_t i=0;i<used_;++i) {
      const int64_t delta=int64_t(history_[i].capturedUs)+offset_-int64_t(hallUs);
      const uint64_t distance=delta<0?uint64_t(-delta):uint64_t(delta);
      if(distance<best){best=distance;entry=&history_[i];refAlignmentUs_=delta;}
    }
    // Include unavailable entries in nearest selection. Do not silently anchor
    // to an older healthy sample when the measurement at the Hall was invalid.
    return entry && best<=ALIGN_US && reference_.synchronize(mm,hallMs,entry->point,odo_);
  }
  const IrOdometryEpoch& odometry() const {return odo_;}
  const MmDistanceReference& reference() const {return reference_;}
  const IrInstrumentState& health() const {return health_;}
  uint32_t revision() const {return revision_;}
  uint32_t accepted() const {return accepted_;}
  uint32_t duplicates() const {return duplicates_;}
  uint32_t rejected() const {return rejected_;}
  uint32_t starts() const {return starts_;}
  uint32_t ends() const {return ends_;}

  int format(char* out,size_t size,uint64_t now,uint64_t locoBoot) {
    tick(now);
    const auto distance=odo_.epochDistance();const auto fromMm=reference_.distanceFromMm(odo_);
    char age[24]="null",travel[40]="null",mmTravel[40]="null",mm[8]="null",hall[16]="null";
    char pulses[24]="null",dt[24]="null";
    const bool fresh=have_ && now>=lastArrival_ && now-lastArrival_<=FRESH_US;
    if(have_ && now>=lastArrival_)snprintf(age,sizeof(age),"%llu",(unsigned long long)((now-lastArrival_)/1000));
    if(distance.available)snprintf(travel,sizeof(travel),"%.3f",distance.mm);
    if(fromMm.available) {
      snprintf(mmTravel,sizeof(mmTravel),"%.3f",fromMm.mm);
      snprintf(mm,sizeof(mm),"%u",unsigned(reference_.mm()));
      snprintf(hall,sizeof(hall),"%lu",(unsigned long)reference_.hallDetectedAtMs());
    }
    if(deltaAvailable_ && odo_.haveMeasurement()) {
      snprintf(pulses,sizeof(pulses),"%llu",(unsigned long long)deltaPulses_);
      snprintf(dt,sizeof(dt),"%llu",(unsigned long long)deltaUs_);
    }
    return snprintf(out,size,
      "{\"shadow\":1,\"loco_boot\":\"%016llX\",\"health\":\"%s\",\"readiness\":\"%s\","
      "\"fresh\":%u,\"age_ms\":%s,\"detector_reason\":%u,\"epoch\":%llu,\"epoch_active\":%u,"
      "\"epoch_starts\":%lu,\"epoch_ends\":%lu,\"epoch_reason\":\"%s\","
      "\"ir_boot\":\"%016llX\",\"seq\":%lu,\"pulses\":%llu,\"epoch_mm\":%s,"
      "\"delta_pulses\":%s,\"delta_us\":%s,\"ref_valid\":%u,\"ref_mm\":%s,\"ref_ms\":%s,"
      "\"ref_event\":%lu,\"ref_alignment_us\":%lld,\"ref_basis\":\"NAV05_ACCEPTED\",\"distance_mm\":%s,"
      "\"accepted\":%lu,\"rejected\":%lu,\"duplicate\":%lu,\"old\":%lu,\"queue_gaps\":%lu,\"changes\":%lu}",
      (unsigned long long)locoBoot,irHealthName(health_.fault),irReadinessName(health_.readiness),
      fresh?1u:0u,age,unsigned(health_.detectorReason),(unsigned long long)odo_.epochId(),odo_.epochActive()?1u:0u,
      (unsigned long)starts_,(unsigned long)ends_,irEpochBreakName(lastBreak_),
      (unsigned long long)last_.bootId,(unsigned long)last_.sequence,(unsigned long long)last_.completedPulses,travel,
      pulses,dt,fromMm.available?1u:0u,mm,hall,(unsigned long)refEvent_,(long long)refAlignmentUs_,mmTravel,
      (unsigned long)accepted_,(unsigned long)rejected_,(unsigned long)duplicates_,(unsigned long)old_,
      (unsigned long)queueGaps_,(unsigned long)revision_);
  }
 private:
  struct Entry {uint64_t capturedUs=0;IrOdometryPoint point{};};
  void setHealth(IrInstrumentState state) {
    if(state.fault!=health_.fault || state.readiness!=health_.readiness || state.healthy!=health_.healthy)++revision_;
    health_=state;
  }
  void apply(const IrOdometryEpoch::Update& u) {
    if(u.epochStarted){++starts_;++revision_;}
    if(u.epochEnded){++ends_;++revision_;}
    if(u.epochStarted || u.epochEnded)lastBreak_=u.reason;
  }
  void fail(IrHealthFault fault,IrEpochBreak reason) {
    auto state=health_;state.healthy=false;state.readiness=IrReadiness::Unavailable;state.fault=fault;
    setHealth(state);apply(odo_.endEpoch(reason));deltaAvailable_=false;
  }
  void reject(){++rejected_;fail(IrHealthFault::PacketInvalid,IrEpochBreak::InstrumentUnavailable);}
  IrOdometryEpoch odo_;
  MmDistanceReference reference_;
  IrInstrumentState health_{};
  IrEpochBreak lastBreak_=IrEpochBreak::None;
  Entry history_[64]{};
  WireSnapshot last_{};
  uint8_t mac_[6]{},head_=0,used_=0,retiredCount_=0;
  uint64_t retired_[8]{},lastArrival_=0,acceptAfter_=0,deltaPulses_=0,deltaUs_=0;
  int64_t offset_=0,refAlignmentUs_=0;
  bool paired_=false,have_=false,exhausted_=false,deltaAvailable_=false;
  uint32_t revision_=0,accepted_=0,rejected_=0,duplicates_=0,old_=0,queueGaps_=0;
  uint32_t starts_=0,ends_=0,refEvent_=0;
};
} // namespace ngr_nav
