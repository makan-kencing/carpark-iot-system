#pragma once

#include <esp_err.h>

esp_err_t zcl_utility_send_update_cmd(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attribute_id);