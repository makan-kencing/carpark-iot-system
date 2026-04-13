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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl_utility.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>
#include <ultrasonic.h>
#include <esp_err.h>

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

static const char *TAG = "MAIN";

static esp_err_t deferred_driver_init(void) {
    return ESP_OK;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG,
                        "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Initialize Zigbee stack");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
                ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
                if (esp_zb_bdb_is_factory_new()) {
                    ESP_LOGI(TAG, "Start network steering");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    ESP_LOGI(TAG, "Device rebooted");
                }
            } else {
                /* commissioning failed */
                ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
            }
            break;
        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                esp_zb_get_extended_pan_id(extended_pan_id);
                ESP_LOGI(
                    TAG,
                    "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                    extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                    extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                    esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            } else {
                ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
                                       ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;
        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                     esp_err_to_name(err_status));
            break;
    }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                        "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
             message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
#ifdef PLACEHOLDER_CODE
    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type ==
                ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                light_state = message->attribute.data.value ? *(bool *) message->attribute.data.value : light_state;
                ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
                light_driver_set_power(light_state);
            }
        }
    }
#endif
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *) message);
            break;
        default:
            ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
            break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters) {
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    esp_zb_ep_list_t *esp_zb_on_off_light_ep = esp_zb_on_off_light_ep_create(HA_ESP_LIGHT_ENDPOINT, &light_cfg);

    zcl_basic_manufacturer_info_t info = {
        .manufacturer_name = ESP_MANUFACTURER_NAME,
        .model_identifier = ESP_MODEL_IDENTIFIER,
    };

    esp_zcl_utility_add_ep_basic_manufacturer_info(esp_zb_on_off_light_ep, HA_ESP_LIGHT_ENDPOINT, &info);
    esp_zb_device_register(esp_zb_on_off_light_ep);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

#define MAX_DISTANCE_CM 20
#define LED_RED_GPIO 13
#define LED_GREEN_GPIO 12
#define TRIGGER_GPIO 4
#define ECHO_GPIO_0 7
#define ECHO_GPIO_1 10
#define ECHO_GPIO_2 5


static bool space_status[3] = {false, false,false};
static float baseline_distance[3] = {0, 0 ,0};

static void update_led(void) {
    bool all_occupied = space_status[0] && space_status[1] && space_status[2] ;
    if (all_occupied) {
        gpio_set_level(LED_RED_GPIO, 1);
        gpio_set_level(LED_GREEN_GPIO, 0);
        ESP_LOGI(TAG, "ALL SPACES FULL");
    } else {
        gpio_set_level(LED_RED_GPIO, 0);
        gpio_set_level(LED_GREEN_GPIO, 1);
        ESP_LOGI(TAG, "SPACE AVAILABLE");
    }
}

static void on_space_occupied(int ultrasonic_id) {
    ESP_LOGI(TAG, "Space %d is OCCUPIED", ultrasonic_id);
    space_status[ultrasonic_id - 1] = true;
    update_led();
}

static void on_space_available(int ultrasonic_id) {
    ESP_LOGI(TAG, "Space %d is AVAILABLE", ultrasonic_id);
    space_status[ultrasonic_id - 1] = false;
    update_led();
}

static int get_echo_pin(int sensor_id) {
    switch (sensor_id) {
        case 1: return ECHO_GPIO_0;
        case 2: return ECHO_GPIO_1;
        case 3: return ECHO_GPIO_2;
        default: return ECHO_GPIO_0;
    }
}

static void parking_ultrasonic_sensor(void* pvParameters) {
    int sensor_id = (int) pvParameters;

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = get_echo_pin(sensor_id)
    };
    ultrasonic_init(&sensor);

    ESP_LOGI(TAG, "Sensor %d: Calibrating...", sensor_id);
    float total = 0;
    int success_count = 0;

    while (success_count < 5) {
        float cal_distance;
        esp_err_t cal_res = ultrasonic_measure(&sensor, 500, &cal_distance);
        if (cal_res == ESP_OK) {
            float cm = cal_distance * 100;
            if (cm > 5.0f) {
                total += cm;
                success_count++;
                ESP_LOGI(TAG, "Sensor %d: Reading %d = %.2f cm",
                         sensor_id, success_count, cm);
            } else {
                ESP_LOGW(TAG, "Sensor %d: Ignoring noise %.2f cm",
                         sensor_id, cm);
            }
        } else {
            ESP_LOGW(TAG, "Sensor %d: Calibration failed, retrying...", sensor_id);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    baseline_distance[sensor_id - 1] = total / 5;
    ESP_LOGI(TAG, "Sensor %d: Final baseline = %.2f cm",
             sensor_id, baseline_distance[sensor_id - 1]);

    while (true) {
        float distance;
        esp_err_t res = ultrasonic_measure(&sensor, 500, &distance);

        if (res != ESP_OK) {
            switch (res) {
                case ESP_ERR_ULTRASONIC_PING:
                    printf("Sensor %d: Cannot ping\n", sensor_id);
                    break;
                case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
                    printf("Sensor %d: Ping timeout\n", sensor_id);
                    break;
                case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
                    printf("Sensor %d: Echo timeout\n", sensor_id);
                    break;
                default:
                    printf("Sensor %d: %s\n", sensor_id, esp_err_to_name(res));
            }
        } else {
            float distance_cm = distance * 100;
            float threshold = baseline_distance[sensor_id - 1] - 0.5f;

            printf("Sensor %d: %.2f cm (baseline: %.2f cm, threshold: %.2f cm)\n",
                   sensor_id, distance_cm,
                   baseline_distance[sensor_id - 1], threshold);

            if (distance_cm < threshold && !space_status[sensor_id - 1]) {
                on_space_occupied(sensor_id);
            } else if (distance_cm >= threshold && space_status[sensor_id - 1]) {
                on_space_available(sensor_id);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void) {
    gpio_set_direction(LED_RED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_RED_GPIO, 0);
    gpio_set_level(LED_GREEN_GPIO, 1);

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
    xTaskCreate(parking_ultrasonic_sensor, "sensor_1", 4096, (void *)1, 5, NULL);
    xTaskCreate(parking_ultrasonic_sensor, "sensor_2", 4096, (void *)2, 5, NULL);
    xTaskCreate(parking_ultrasonic_sensor, "sensor_3", 4096, (void *)3, 5, NULL);
}
