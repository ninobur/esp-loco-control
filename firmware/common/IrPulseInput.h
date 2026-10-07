#pragma once
#include "../programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/PulseEventEvidence.h"

// One explicit build setting for BOTH IR and Otto images. No runtime fallback.
// DIRECT establishes the boundary only: its hardware producer is not implemented.
#define NGR_IR_INPUT_DIRECT 0
#define NGR_IR_INPUT_WIRELESS 1
#ifndef NGR_IR_INPUT_PATH
#define NGR_IR_INPUT_PATH NGR_IR_INPUT_WIRELESS
#endif
#if NGR_IR_INPUT_PATH != NGR_IR_INPUT_DIRECT && NGR_IR_INPUT_PATH != NGR_IR_INPUT_WIRELESS
#error "NGR_IR_INPUT_PATH must be NGR_IR_INPUT_DIRECT or NGR_IR_INPUT_WIRELESS"
#endif

namespace ir_input {
constexpr bool Wireless=NGR_IR_INPUT_PATH==NGR_IR_INPUT_WIRELESS;
constexpr const char* Name=Wireless?"WIRELESS":"DIRECT";
// Reuse the proven native bytes, independently of ESP-NOW envelopes and ACKs.
using NativeEvidence=PulseEventPacket;
struct Arrival {
  NativeEvidence evidence{};
  uint64_t receivedUs=0;
  uint32_t queueDrops=0;
  uint8_t sourceMac[6]{}; // Wireless provenance; zero for a future onboard producer.
};
}
