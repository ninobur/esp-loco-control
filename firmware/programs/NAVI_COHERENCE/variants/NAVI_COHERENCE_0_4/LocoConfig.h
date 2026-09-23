#pragma once
#include <Arduino.h>
struct PwmSpeedEntry { uint8_t pwm; float pKph; };
// First target is Toby. A different locomotive needs a separate reviewed build.
#include "../../../NAVI_SIMPLIFIED/LL_LocoConfig_9950012.h"
#ifndef NGR_ENABLE_EXPERIMENTAL_AUTO
#define NGR_ENABLE_EXPERIMENTAL_AUTO 0
#endif
// No approved physical maximum-speed bound is installed by this experiment.
static constexpr uint32_t NGR_PHYSICAL_VMAX_MM_S=0;
