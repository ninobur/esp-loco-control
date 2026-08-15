// Host shim for esp_now.h — replay harness only, never flashed.
// The replay has no radio: init reports success, sends are counted and
// dropped, no callback ever fires. The CTO3 layer therefore sees a railway
// with no peers — exactly a solo session — and every navigation fixture
// replays unchanged. Peer-coordination behaviour is NOT exercised here; it
// needs two locomotives (spec §10) or a future two-instance harness.
#pragma once
#include <Arduino.h>
#include <string.h>

#define ESP_OK 0
typedef int esp_err_t;

typedef struct { uint8_t peer_addr[6]; uint8_t channel; bool encrypt; } esp_now_peer_info_t;
typedef struct { uint8_t* src_addr; } esp_now_recv_info_t;
typedef void (*esp_now_recv_cb_t)(const esp_now_recv_info_t*, const uint8_t*, int);
// 1.16Ra: the send-callback status enum and signature. The harness has no
// radio, so no callback ever fires and ctoTxDone/ctoTxFailed stay 0 — which
// is the honest replay answer: a replay transmits nothing.
typedef enum { ESP_NOW_SEND_SUCCESS=0, ESP_NOW_SEND_FAIL=1 } esp_now_send_status_t;
// Core 3.3.11 passes wifi_tx_info_t*, matching the recv callback's move to
// esp_now_recv_info_t*. The shim only needs the type to exist.
typedef struct { uint8_t des_addr[6]; } wifi_tx_info_t;
typedef void (*esp_now_send_cb_t)(const wifi_tx_info_t*, esp_now_send_status_t);

static inline esp_err_t esp_now_init(){ return ESP_OK; }
static inline esp_err_t esp_now_add_peer(const esp_now_peer_info_t*){ return ESP_OK; }
static inline bool      esp_now_is_peer_exist(const uint8_t*){ return false; }
static inline esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t){ return ESP_OK; }
static inline esp_err_t esp_now_register_send_cb(esp_now_send_cb_t){ return ESP_OK; }
static inline esp_err_t esp_now_send(const uint8_t*, const uint8_t*, size_t){ return ESP_OK; }
