// Audit witnesses only. Includes unchanged candidate headers; never controls hardware.
#include "NaviIntegratedCore.h"
#include "NaviTileTiming.h"
#include "NaviStations.h"
#include "NaviCompatibility.h"
#include "NaviResourceDiagnostics.h"
#include "NaviEstop.h"
#include <cassert>
#include <iostream>
#include <cstring>
using namespace navi_eyes;

static ir_movement::WireSnapshot packet(unsigned seq, uint64_t us, uint64_t pulses) {
  ir_movement::WireSnapshot w;
  w.bootId=7;w.sequence=seq;w.capturedUs=us;
  w.completedPulses=w.observedRises=pulses;
  w.nominalUm=pulses*w.pitchUm;w.opticalReason=ir_movement::TRACKING;
  return w;
}

// Only shell services inserted below are extracted verbatim from the candidate.
// These mocks record requested output; they do not simulate motor physics or RTOS.
static NaviIntegratedCore navi;
static bool autoRunning=false,autoEnrolled=false,estopAsserted=false,estopped=false;
static OrderedEstop orderedEstop;
static int estopMux=0,requested=90;
static constexpr unsigned AUTO_STEP_DOWN_MS=31;
static void requestPwm(int target,unsigned,unsigned){requested=target;}
static bool pub(const char*,const char*,bool){return true;}
static void warn(const char*,bool){}
static void serviceRamp(){}
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
// AUDIT_INSERT_VERBATIM_SERVICES

int main(){
  {
    NaviIntegratedCore c;c.declare(108,1,1);
    auto w=packet(1,100000,10);c.observeIr(w,100000,80);
    w=packet(2,200000,11);++w.sampleGaps;w.opticalReason=ir_movement::SAMPLE_GAP;
    c.observeIr(w,200000,0);
    assert(!c.irApplicable(200000) && c.pwmZeroMovementRequiresDeclaration());
    char message[512];formatConsoleNav(message,sizeof(message),c,1,200000);
    std::cout<<"invalid_optics_pwm0_hold=1 ir_applicable=0 position_reliable="<<c.positionReliable()<<" console="<<message<<'\n';
    assert(std::strstr(message,"NORMAL") && std::strstr(message,"RVM_IR_UNAVAILABLE"));
  }
  {
    NaviIntegratedCore c;auto a=packet(10,1000000,100);c.observeIr(a,1000000,80);
    auto old=packet(9,900000,90);c.observeIr(old,1100000,80);
    assert(!c.irApplicable(1100000));
    c.observeIr(a,1200000,80);
    assert(c.irApplicable(1200000));
    std::cout<<"replayed_pre_fault_high_water_packet_accepted=1 epoch="<<c.irMeasurementEpochId()<<'\n';
  }
  {
    NaviIntegratedCore c;c.declare(121,-1,1);
    StopView view;view.identityAvailable=true;view.identifiedMm=120;view.identifiedDirection=-1;
    NaviTileTiming timing;
    auto out=timing.command(view,1,1000000,1000000,stopMarkerUm(108),c.travelHistory(120,-1),90,90,90);
    assert(out.missingHistory);
    std::cout<<"ccw_mm120_arches_missing_history=1 timing_ready="<<c.timingReferenceReady()<<" table_direction="<<int(c.timingReferenceDirection())<<'\n';
  }
  {
    StopView view;view.identityAvailable=true;view.identifiedMm=14;view.identifiedDirection=1;
    NaviTileTiming timing;NaviStopOverlay overlay;
    const auto& history=startup_lap::kIntervals[14];
    auto out=timing.command(view,1,1000000,1000000,stopMarkerUm(15),history,90,90,50);
    assert(out.entryNow && out.geography.section==StopSection::Penultimate);
    out=timing.command(view,1,1000000,1000000+history.intervalUs,stopMarkerUm(15),history,90,90,out.entryPwm);
    assert(out.geography.pwm==0 && out.geography.section==StopSection::Penultimate);
    auto stop=overlay.command(out.geography,out.available,1,90,0,0,30000,true);
    stop=overlay.command(out.geography,out.available,1,90,0,0,40000,true);
    assert(stop.pwm==0 && stop.execution==StopExecution::Geographic && !stop.releasedNow);
    std::cout<<"cw_mm14_timed_pwm="<<int(stop.pwm)<<" section="<<int(out.geography.section)<<" execution="<<int(stop.execution)<<" release_after_10s=0 duration_us="<<history.intervalUs<<'\n';
  }
  {
    navi.declare(108,1,1);navi.requestStop(DartStop::NoApplicableReference);
    autoEnrolled=autoRunning=false;requested=90;serviceNavigationStop();
    assert(requested==0);
    std::cout<<"navigation_stop_requests_zero_in_manual=1\n";
  }
  {
    navi_resource::Console c;char row[700];std::memset(row,'X',sizeof(row)-1);row[699]=0;
    c.printf("[RVM] %s\n",row);navi_resource::ConsoleLine line;assert(c.take(line));
    assert(c.truncated()==1 && line.length==255);
    std::cout<<"rvm_sized_console_row_truncated=1 retained_bytes="<<line.length<<'\n';
  }
}
