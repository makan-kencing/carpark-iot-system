/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier:  LicenseRef-Included
 *
 * Zigbee HA_on_off_light Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once

#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false                                /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN        /* aging timeout of device */
#define ED_KEEP_ALIVE                   3000                                 /* 3000 millisecond */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

#define MANUFACTURER_NAME               "\x09""ESPRESSIF"

#if CONFIG_SMART_GATE_EXIT
#define MODEL_IDENTIFIER                "\x04""SGEX"
#elif CONFIG_SMART_GATE_ENTRY
#define MODEL_IDENTIFIER                "\x04""SGEN"
#else
#error "unsupported smart gate"
#endif

#define APP_PROFILE_ID                  ESP_ZB_AF_HA_PROFILE_ID
#define POWER_SOURCE                    1                                     /* 0x01     ==  External power supply                   */

#define HA_ESP_ENDPOINT                 1
#define HA_CONTROL_CLUSTER              0xfd10
#define HA_CONTROL_GATE_ATTR            0x0000
#define HA_CONTROL_DISPLAY_ATTR         0x0001
#define HA_CONTROL_NFC_ATTR             0x0002
#define HA_CONTROL_CLEAR_DISPLAY_CMD    0x0000
#define HA_CONTROL_CLEAR_NFC_CMD        0x0001


#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }
