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
#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false                                /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN        /* aging timeout of device */
#define ED_KEEP_ALIVE                   3000                                 /* 3000 millisecond */
#define HA_ESP_LIGHT_ENDPOINT           10                                   /* esp light bulb device endpoint, used to process light controlling commands */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07"CONFIG_IDF_TARGET /* Customized model identifier */

/* Servo Gate Configuration */
#define SERVO_GPIO               3     /* ESP32-C6 safe GPIO */
#define SERVO_MIN_PULSEWIDTH_US  500   /* 0 degrees pulse width */
#define SERVO_MAX_PULSEWIDTH_US  2500  /* 180 degrees pulse width */
#define SERVO_MAX_DEGREE         180   /* Max physical angle */
#define SERVO_PERIOD_US          20000 /* 50Hz frequency (20ms) */

#define LEDC_TIMER               LEDC_TIMER_0
#define LEDC_MODE                LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL             LEDC_CHANNEL_0
#define LEDC_DUTY_RES            LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY           50

#define GATE_OPEN_ANGLE          90 /* Angle for Open gate */
#define GATE_CLOSED_ANGLE        0  /* Angle for Closed gate */

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