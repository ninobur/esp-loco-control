// Read-only forensic adapter. Access changes visibility in this host translation
// unit; the preserved observer source and its executed arithmetic are unchanged.
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#define private public
#include "WaveformObserver.h"
#undef private
#include "FixedReference.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace manual_rvm;
struct Node { int parent,frame,step,part; double time; float x,hall,mean,scale,loss,motion,prediction,cost; double totalHall=0,totalMotion=0,baseHall=0,baseMotion=0,anchorCost=0; };
int main(int argc,char** argv) {
  assert(argc==3);
  static WaveformObserver w(fixed_reference::cw,fixed_reference::ccw);
  std::ifstream input(argv[1]);std::string line;std::getline(input,line);
  std::string out=argv[2];
  std::ofstream costs(out+"/commitment_costs.csv");
  costs<<std::setprecision(12)<<"time_s,phase,part,completed_mm,total_relative_cost,anchor_cost,hall_since_anchor,motion_since_anchor,done_evidence,done_quality,within_one_cost,retained_after_commit,pulse_peak_part,pulse_peak_phase\n";
  std::ofstream frames(out+"/frames.csv"), path(out+"/path_to_153.csv"), refs(out+"/reference.csv"), anchors(out+"/anchors.csv");
  frames<<std::setprecision(10);path<<std::setprecision(10);anchors<<std::setprecision(10);
  frames<<"time_s,hall,corrected_hall,pwm,pwm_distance,phase,phase_change,step_mm,prediction_mm,part,done_part,done_evidence,done_quality,uncertainty,recognized,accepted,reference_mean,reference_scale,emission,motion,anchor_low,anchor_high,band_low\n";
  anchors<<"time_s,mm,phase,low,high,evidence,quality,pwm_distance\n";
  refs<<"position_mm,mean,scale,part,baseline\n";
  std::vector<Node> nodes;std::vector<int> oldNodes(w.capacity,-1);
  int frame=0,selected=-1;float previousPhase=0;
  while(std::getline(input,line)) {
    std::stringstream ss(line);std::vector<double> v;std::string field;
    while(std::getline(ss,field,','))v.push_back(std::stod(field));
    Observation o{uint64_t(v[3]),float(v[4]),uint8_t(v[5]),bool(v[7])};
    if(!frame) {w.declare(146,1,uint64_t(v[2])-1);previousPhase=w.report().phase;}
    const int lo=w.lo_;
    float elapsed=float(o.us-w.lastUs_)*1e-6f,dt=std::min(elapsed,.1f);
    float speed=o.pwm?w.m_->speeds[52+std::min(51,int(o.pwm)/5)]:0;
    float prediction=speed*dt/w.m_->gridMm;
    int maxStep=o.pwm?std::min(20,std::max(3,int(std::ceil(1000*dt/w.m_->gridMm)))):0;
    float offset=w.offsetReady_?w.offset_:o.hall-w.baseline(w.report().next);
    float y=o.hall-offset;
    bool gap=o.gap||elapsed>.1f;
    std::vector<WaveformObserver::State> candidateStates(w.capacity);
    std::vector<int> temp(w.capacity,-1);std::vector<float> scores(w.capacity);
    float best=std::numeric_limits<float>::infinity();int winner=-1;
    for(int i=0;i<w.capacity;++i) {
      Cell c=w.cellAt(w.position(i));float mean=c.mean4*.25f,scale=std::max(1.5f,c.scale4*.25f);
      float emission=gap?0:w.loss((y-mean)/scale),score=std::numeric_limits<float>::infinity(),motion=0;int parent=-1,stepChosen=0;
      for(int step=0;step<=maxStep&&step<=i;++step) {
        float m=.08f*std::min(2.f,std::fabs(step-prediction));
        float candidate=w.a_[i-step].cost+emission+0.08f*std::min(2.f,std::fabs(step-prediction));
        if(candidate<score) {score=candidate;parent=i-step;stepChosen=step;motion=m;}
      }
      scores[i]=score;
      if(parent>=0) {
        temp[i]=int(nodes.size());
        nodes.push_back({oldNodes[parent],frame,stepChosen,w.parts_[i],double(o.us)/1e6,w.position(i),y,mean,scale,emission,motion,prediction,score});
        Node& n=nodes.back();
        if(n.parent>=0) {
          const Node& p=nodes[n.parent];
          n.totalHall=p.totalHall;n.totalMotion=p.totalMotion;
          n.baseHall=p.baseHall;n.baseMotion=p.baseMotion;n.anchorCost=p.anchorCost;
        }
        n.totalHall+=emission;n.totalMotion+=motion;
        auto state=w.a_[parent];
        if(state.part!=w.parts_[i]) {
          state.donePart=state.part;state.doneEvidence=state.contrast;
          state.doneQuality=state.weight>0?state.sum/state.weight:8;
        }
        candidateStates[i]=state;
      }
      if(score<best) {best=score;winner=i;}
    }
    w.observe(o);assert(winner>=0);
    assert(w.report().phase==float((lo+winner)*5));
    for(int i=0;i<w.capacity;++i) {
      const int old=w.lo_+i-lo;
      if(std::isfinite(w.a_[i].cost)) {
        assert(old>=0&&old<w.capacity);
        if(std::fabs(w.a_[i].cost-(scores[old]-best))>=1e-5f) { std::cerr<<"cost mismatch frame="<<frame<<" i="<<i<<" original="<<w.a_[i].cost<<" traced="<<scores[old]-best<<"\n"; return 2; }
        oldNodes[i]=temp[old];
      } else oldNodes[i]=-1;
    }
    const Report& r=w.report();const Node& n=nodes[temp[winner]];
    const auto& state=w.a_[int(r.phase/5)-w.lo_];
    frames<<n.time<<","<<o.hall<<","<<y<<","<<int(o.pwm)<<","<<r.pwmDistance<<","<<r.phase<<","<<r.phase-previousPhase<<","<<n.step*5<<","<<n.prediction*5<<","<<n.part<<","<<state.donePart<<","<<r.evidence<<","<<r.quality<<","<<r.uncertainty<<","<<r.mm<<","<<r.changed<<","<<n.mean<<","<<n.scale<<","<<n.loss<<","<<n.motion<<","<<r.anchorLow<<","<<r.anchorHigh<<","<<w.lo_*5<<"\n";
    previousPhase=r.phase;
    if(r.changed && (r.mm==150 || r.mm==153)) {
      for(int i=0;i<w.capacity;++i)if(temp[i]>=0) {
        Node& node=nodes[temp[i]];
        if(r.mm==150) {node.baseHall=node.totalHall;node.baseMotion=node.totalMotion;node.anchorCost=scores[i]-best;}
        const auto& state=candidateStates[i];
        int after=lo+i-w.lo_;bool retained=after>=0&&after<w.capacity&&std::isfinite(w.a_[after].cost);
        int peakNode=temp[i];
        while(peakNode>=0 && nodes[peakNode].time>50.1995651) peakNode=nodes[peakNode].parent;
        int peakPart=peakNode>=0?nodes[peakNode].part:-1;
        float peakPhase=peakNode>=0?nodes[peakNode].x:-1;
        costs<<n.time<<","<<node.x<<","<<node.part<<","<<state.donePart<<","<<scores[i]-best<<","<<node.anchorCost<<","<<node.totalHall-node.baseHall<<","<<node.totalMotion-node.baseMotion<<","<<state.doneEvidence<<","<<state.doneQuality<<","<<(scores[i]-best<=1)<<","<<retained<<","<<peakPart<<","<<peakPhase<<"\n";
      }
    }
    if(r.changed)anchors<<n.time<<","<<r.mm<<","<<r.phase<<","<<r.anchorLow<<","<<r.anchorHigh<<","<<r.evidence<<","<<r.quality<<","<<r.pwmDistance<<"\n";
    if(r.changed&&r.mm==153&&selected<0)selected=temp[winner];
    ++frame;
  }
  std::vector<int> lineage;for(int n=selected;n>=0;n=nodes[n].parent)lineage.push_back(n);
  std::reverse(lineage.begin(),lineage.end());
  path<<"time_s,phase,part,step_mm,prediction_mm,hall,reference_mean,reference_scale,emission,motion,relative_cost_before_normalization\n";
  for(int id:lineage){auto& n=nodes[id];path<<n.time<<","<<n.x<<","<<n.part<<","<<n.step*5<<","<<n.prediction*5<<","<<n.hall<<","<<n.mean<<","<<n.scale<<","<<n.loss<<","<<n.motion<<","<<n.cost<<"\n";}
  for(int x=45000;x<47500;x+=5){auto c=w.cellAt(x);int p=w.partAt(x);refs<<x<<","<<c.mean4*.25<<","<<c.scale4*.25<<","<<p<<","<<w.baseline(p)<<"\n";}
  std::cout<<"Verified exact phase and every retained-state cost for "<<frame<<" input blocks; "<<lineage.size()<<" ancestors of first MM153 acceptance.\n";
  for(int mm=149;mm<=154;++mm)std::cout<<mm<<" peak="<<w.m_->peaks[mm]<<" seam="<<w.m_->seams[mm]<<" baseline="<<w.baseline(mm)<<"\n";
}
