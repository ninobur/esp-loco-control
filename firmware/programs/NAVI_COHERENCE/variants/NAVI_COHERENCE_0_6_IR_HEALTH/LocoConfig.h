#pragma once
#include <Arduino.h>
struct PwmSpeedEntry { uint8_t pwm; float pKph; };
// First target is Toby. A different locomotive needs a separate reviewed build.
#include "../../../NAVI_SIMPLIFIED/LL_LocoConfig_9950012.h"
// 20Q3 (Twenty Questions handoff 11): Hall opening = absolute departure >=70
// counts from the locked X22 interval baseline on two consecutive same-sign
// 1 kHz samples, detected on the second. Provisional, field-supported value;
// replaces this build's inherited 25+13=38. The shared NAVI_SIMPLIFIED
// profile is deliberately untouched (other sketches include it).
#define NAVI_HALL_DEPART_COUNTS 70
#ifndef NGR_ENABLE_EXPERIMENTAL_AUTO
#define NGR_ENABLE_EXPERIMENTAL_AUTO 0
#endif
// No approved physical maximum-speed bound is installed by this experiment.
static constexpr uint32_t NGR_PHYSICAL_VMAX_MM_S=0;
