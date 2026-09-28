#pragma once
#include <Arduino.h>
struct PwmSpeedEntry { uint8_t pwm; float pKph; };
// First target is Toby. A different locomotive needs a separate reviewed build.
#include "../../../NAVI_SIMPLIFIED/LL_LocoConfig_9950012.h"
// EYES_WIDE_OPEN: the MM opening (>=70 counts from NAVI's Hall reference on
// two consecutive same-sign samples) and the 100/200 mm spatial-reference
// boundaries are defined once, in NaviHall.h, so host tests and the sketch
// share them. The shared NAVI_SIMPLIFIED profile is deliberately untouched;
// its NAVI_BASELINE_ADAPT_PWM (an X22 motion proxy) is not used by this build.
#ifndef NGR_ENABLE_EXPERIMENTAL_AUTO
#define NGR_ENABLE_EXPERIMENTAL_AUTO 0
#endif
// No approved physical maximum-speed bound is installed by this experiment.
static constexpr uint32_t NGR_PHYSICAL_VMAX_MM_S=0;
