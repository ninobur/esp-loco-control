#pragma once
// ============================================================================
// ESPNOW_CMD_TX key material — TEMPLATE.
// Copy to espnow_keys.h (gitignored), fill real values, keep out of git.
// The SAME PMK/LMK values must be compiled into the locomotive RX side.
// Generate 16 random bytes per key:  openssl rand -hex 16
// A locomotive MAC is its STA MAC (printed by the loco at boot, or from
// the router's client list / arp -a against its IP).
// ============================================================================
#define NGR_ESPNOW_PMK  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}

// One entry per locomotive peer: {name, loco_id, STA MAC, LMK}
// All-zero MAC or all-zero LMK = placeholder -> the bridge FAILS CLOSED.
#define NGR_ESPNOW_PEERS \
  { {"Otto", 9950011UL, {0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}, \
    {"Toby", 9950012UL, {0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}} }
