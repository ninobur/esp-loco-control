#pragma once
#include <stdint.h>
#define ESP_OK 0
typedef int esp_err_t;
typedef struct { uint8_t peer_addr[6]; int channel; bool encrypt; } esp_now_peer_info_t;
typedef struct { const uint8_t* src_addr; } esp_now_recv_info_t;
typedef struct { int dummy; } wifi_tx_info_t;
typedef enum { ESP_NOW_SEND_SUCCESS=0, ESP_NOW_SEND_FAIL } esp_now_send_status_t;
static inline esp_err_t esp_now_init(){ return ESP_OK; }
static inline bool esp_now_is_peer_exist(const uint8_t*){ return false; }
static inline esp_err_t esp_now_add_peer(const esp_now_peer_info_t*){ return ESP_OK; }
typedef void (*esp_now_recv_cb_t)(const esp_now_recv_info_t*, const uint8_t*, int);
typedef void (*esp_now_send_cb_t)(const wifi_tx_info_t*, esp_now_send_status_t);
static inline esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t){ return ESP_OK; }
static inline esp_err_t esp_now_register_send_cb(esp_now_send_cb_t){ return ESP_OK; }
static inline esp_err_t esp_now_send(const uint8_t*, const uint8_t*, size_t){ return ESP_OK; }
