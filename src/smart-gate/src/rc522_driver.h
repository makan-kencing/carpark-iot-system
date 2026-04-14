#pragma once
#include <esp_err.h>

typedef enum esp_nfc_callback_action_s {
    ESP_NFC_READ = 0,
    ESP_NFC_REMOVE = 1
} esp_nfc_callback_action_t;

typedef struct esp_nfc_callback_message_read_s {
    char* data;
    size_t size;
} esp_nfc_callback_message_read_t;

typedef struct esp_nfc_callback_message_remove_s {
} esp_nfc_callback_message_remove_t;

typedef void (*esp_nfc_callback_t)(esp_nfc_callback_action_t callback_id, void* message);

void rc522_driver_init(esp_nfc_callback_t cb, uint16_t poll_interval_ms);