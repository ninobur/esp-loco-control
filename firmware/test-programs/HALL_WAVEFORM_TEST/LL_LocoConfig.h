#pragma once
// ============================================================================
// LL_LocoConfig.h — profile selector for HALL_WAVEFORM_TEST
//
// INVESTIGATORY / UNAPPROVED diagnostic sketch. Exactly ONE include below may
// be active. The base sketch (LL_PM_Loco_ModuleC_v0_4_1.ino) includes this
// filename, so it is kept rather than renamed to the QUORUM-side LocoConfig.h.
//
// The two profile headers here are copies of firmware/config/, taken so this
// sketch opens and compiles in place from the Arduino IDE like every other
// test program. If a profile value changes in firmware/config/, re-copy it —
// nothing syncs automatically.
//
// Verify after flashing: the boot banner must read the intended locomotive id.
// ============================================================================

#include "LL_LocoConfig_9950011.h"     // Otto
//#include "LL_LocoConfig_9950012.h"   // Toby
