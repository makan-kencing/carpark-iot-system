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
#include "main.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include <string.h>
#include <ultrasonic.h>
#include <stdbool.h>

#include "zcl_utility.h"
#include "led_driver.h"
#include "ultrasonic_sensor.h"

#define countof(p) sizeof(p) / sizeof(*p)

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

#define TOTAL_SPACE 3
static uint8_t remaining_space = TOTAL_SPACE;
static float remaining_space_attr = TOTAL_SPACE;

ultrasonic_sensor_info_t sensors[TOTAL_SPACE] = {
    {{.trigger_pin = CONFIG_TRIGGER_GPIO, .echo_pin = CONFIG_ECHO_GPIO_1}, false, 0},
    {{.trigger_pin = CONFIG_TRIGGER_GPIO, .echo_pin = CONFIG_ECHO_GPIO_2}, false, 0},
    {{.trigger_pin = CONFIG_TRIGGER_GPIO, .echo_pin = CONFIG_ECHO_GPIO_3}, false, 0}
};

static const char *TAG = "MAIN";

static void ultrasonic_sensor_handler(const int8_t delta) {
    remaining_space += delta;

    if (remaining_space == 0) {
        ESP_LOGI(TAG, "ALL SPACES FULL");

        ESP_ERROR_CHECK(gpio_set_level(CONFIG_RED_LED_GPIO, 1));
        ESP_ERROR_CHECK(gpio_set_level(CONFIG_GREEN_LED_GPIO, 0));
    } else {
        ESP_LOGI(TAG, "SPACE AVAILABLE");

        ESP_ERROR_CHECK(gpio_set_level(CONFIG_RED_LED_GPIO, 0));
        ESP_ERROR_CHECK(gpio_set_level(CONFIG_GREEN_LED_GPIO, 1));
    }

    remaining_space_attr = remaining_space;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(
        HA_ESP_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, &remaining_space_attr, false
    );
    zcl_utility_send_update_cmd(
        HA_ESP_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
        ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID
    );
    esp_zb_lock_release();
}

static esp_err_t deferred_driver_init(void) {
    static bool is_inited = false;
    if (!is_inited) {
        set_max_current_value();

        led_driver_init();
        ESP_ERROR_CHECK(ultrasonic_sensor_init(
                &(ultrasonic_sensor_config_t) {
                .sensors = {sensors, countof(sensors)}
                }, ultrasonic_sensor_handler, 1)
        );

        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG,
                        "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_s) {
    const uint32_t *p_sg_p = signal_s->p_app_signal;
    const esp_err_t err_status = signal_s->esp_err_status;
    const esp_zb_app_signal_type_t sig_type = *p_sg_p;
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

static void esp_zb_task(void *pvParameters) {
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();

    esp_zb_analog_input_cluster_cfg_t analog_input_cfg = { .present_value = TOTAL_SPACE };
    esp_zb_basic_cluster_cfg_t basic_cluster_cfg =  {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };

    // --------------------------------- Endpoint 1 -- Basic Cluster -------------------------------------
    /* basic cluster */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&basic_cluster_cfg);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER);

    /* identify cluster */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);

    /* Create customized temperature sensor endpoint */
    esp_zb_attribute_list_t *analog_input_cluster = esp_zb_analog_input_cluster_create(&analog_input_cfg);

    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_analog_input_cluster(esp_zb_cluster_list, analog_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    const esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_ENDPOINT,
        .app_profile_id = APP_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config);
    // --------------------------------------- End Endpoint 1 --------------------------------------------

    esp_zb_device_register(esp_zb_ep_list);

    /* Config the reporting info  */
    esp_zb_zcl_reporting_info_t reporting_info = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .attr_id = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };

    esp_zb_zcl_update_reporting_info(&reporting_info);

    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}


void app_main(void) {
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
