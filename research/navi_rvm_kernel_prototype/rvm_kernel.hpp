#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>

namespace rvm {
struct Cell {float mean, scale;};
struct Model { std::vector<Cell> landscape; std::vector<int> mm_peak; int samples_per_mm=0; };
struct Config {float smoothing=0.15f; float max_phase_step=5.f; float motion_step=1.f; float phase_prior_weight=0.10f; float min_margin=0.18f; int evidence_samples=4; int lookback=18; int ahead=80; int behind=25;};
struct Decision {bool advance=false; int tile=0; float phase=0, score=0, margin=0; bool mismatch=false;};

// A bounded, causal, map-indexed waveform tracker. Not a Hall region detector.
// Every sample updates a persistent phase frontier against the SAME map.
// A tile is committed only when the best path passes its learned landmark
// and the path's score beats the best path remaining before that landmark.
class Kernel {
 Model model_; Config cfg_; std::vector<float> prev_, next_;
 float filtered_=0; bool initialized_=false; int tile_=0; int streak_=0;
 int confirm_=0; float last_phase_=0;
 static float robust(float z){float a=std::fabs(z); return a<2.f?0.5f*z*z:2.f*a-2.f;}
public:
 Kernel(Model m,Config c,int starting_tile):model_(std::move(m)),cfg_(c),tile_(starting_tile){
   prev_.assign(model_.landscape.size(),std::numeric_limits<float>::infinity());
   next_=prev_; if(!model_.mm_peak.empty() && starting_tile>=0 && starting_tile<(int)model_.mm_peak.size()) {
     int p=model_.mm_peak[starting_tile]; if(p>=0 && p<(int)prev_.size()) prev_[p]=0;
   }
 }
 Decision observe(float adc,float applied_pwm){
   Decision out; out.tile=tile_;out.phase=last_phase_;
   if(applied_pwm<=0){return out;} // record PWM-zero externally; never infer geographic motion
   if(!initialized_){filtered_=adc;initialized_=true;}else filtered_+=cfg_.smoothing*(adc-filtered_);
   const int n=(int)prev_.size(); if(n==0||tile_+1>=(int)model_.mm_peak.size())return out;
   int target=model_.mm_peak[tile_+1];
   int lo=std::max(0,(int)last_phase_-cfg_.behind), hi=std::min(n-1,(int)last_phase_+cfg_.ahead);
   std::fill(next_.begin(),next_.end(),std::numeric_limits<float>::infinity());
   float best=std::numeric_limits<float>::infinity(); int best_idx=-1;
   for(int j=lo;j<=hi;j++){
     float z=(filtered_-model_.landscape[j].mean)/std::max(1.f,model_.landscape[j].scale);
     float emission=robust(z);
     for(int step=0;step<=(int)cfg_.max_phase_step;step++){
       int p=j-step;if(p<0)break;
       float prior=cfg_.phase_prior_weight*std::fabs(step-cfg_.motion_step);
       float candidate=prev_[p]+emission+prior;
       if(candidate<next_[j])next_[j]=candidate;
     }
     if(next_[j]<best){best=next_[j];best_idx=j;}
   }
   if(best_idx<0||!std::isfinite(best))return out;
   // Normalize costs: the frontier holds only relative, bounded history.
   for(int j=lo;j<=hi;j++)if(std::isfinite(next_[j]))next_[j]-=best;
   prev_.swap(next_);last_phase_=best_idx;
   float before=std::numeric_limits<float>::infinity(),after=std::numeric_limits<float>::infinity();
   for(int j=lo;j<=hi;j++){
     if(j<target)before=std::min(before,prev_[j]);
     else after=std::min(after,prev_[j]);
   }
   out.phase=last_phase_;out.score=best;out.margin=before-after;
   // Confirmation persists across samples; an early lobe alone cannot commit.
   if(best_idx>=target && out.margin>=cfg_.min_margin)confirm_++;else confirm_=0;
   if(confirm_>=cfg_.evidence_samples){
     ++tile_;out.tile=tile_;out.advance=true;confirm_=0;
     // A suspicious local fit is diagnostic, not a veto. Never reset map phase.
     out.mismatch=(best>3.f);if(out.mismatch)++streak_;else streak_=0;
   }
   return out;
 }
 int mismatches()const{return streak_;}
};
}
