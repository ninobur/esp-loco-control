// ============================================================================
// IR_ESPNOW_SENDER local configuration — NON-SECRET (no credentials belong in
// this sketch at all; spec §3 strips WiFi association entirely).
//
// TOBY_STA_MAC is the one value that must be filled in from hardware. Get it
// from Toby's boot serial or from `ir_status` telemetry once the receiver
// build is flashed. ALL-ZERO IS A SAFE DEFAULT: the sender refuses to start
// transmission (spec §4) and keeps measuring locally, printing its own MAC so
// the operator can complete the pairing in the other direction.
// ============================================================================
#pragma once
#include <stdint.h>

static constexpr uint8_t  IR_ESPNOW_CHANNEL   = 11;          // railway EAP channel (runbook)
static constexpr uint32_t IR_SENSOR_ID        = 0x49523031UL; // "IR01"
static constexpr uint32_t IR_TARGET_LOCO_ID   = 9950012UL;   // Toby
static constexpr uint32_t IR_TX_INTERVAL_MS   = 50;          // 20 reports/s, provisional Stage A
static constexpr uint32_t IR_STOPPED_MS       = 2500;        // no pulse progress -> STOPPED/0

// Toby's WiFi STA MAC. FILL IN AT BENCH PAIRING (spec §11.3 step 1).
static const uint8_t TOBY_STA_MAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
