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
#include <string.h>
#include <sys/types.h>

#include "i2c_driver.h"
#include "lcd_driver.h"
#include "servo_driver.h"
#include "rc522_driver.h"

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

bool gate_state = false;
char display_text_attr[4 * 16];
#if CONFIG_SMART_GATE_EXIT
char nfc_id_attr[RC522_PICC_UID_SIZE_MAX + 2];
#endif

static const char *TAG = "MAIN";

static esp_err_t esp_zb_zcl_send_update_cmd(const uint16_t cluster_id, const uint16_t attribute_id) {
    esp_zb_zcl_report_attr_cmd_t ph_cmd_req = {
        .clusterID = cluster_id,
        .attributeID = attribute_id,
        .address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .zcl_basic_cmd.src_endpoint = HA_ESP_ENDPOINT
    };
    return esp_zb_zcl_report_attr_cmd_req(&ph_cmd_req);
}

static void esp_app_nfc_handler(const esp_nfc_callback_action_t callback_id, const void* message) {
    switch (callback_id) {
        case ESP_NFC_READ:
        case ESP_NFC_REMOVE:
            if (callback_id == ESP_NFC_READ) {
                const esp_nfc_callback_message_read_t* payload = (esp_nfc_callback_message_read_t*) message;
                if (memcmp(nfc_id_attr, payload->picc->uid.value, payload->picc->uid.length * sizeof(uint8_t)) != 0) {
                    nfc_id_attr[0] = payload->picc->uid.length;
                    memcpy(nfc_id_attr + 1, payload->picc->uid.value, payload->picc->uid.length * sizeof(uint8_t));
                    nfc_id_attr[payload->picc->uid.length + 1] = 0;

                    // do other stuff when nfc detected for the first time
                }
            } else {
                nfc_id_attr[0] = 0;
            }

            esp_zb_lock_acquire(portMAX_DELAY);
            esp_zb_zcl_set_attribute_val(HA_ESP_ENDPOINT,
                HA_CONTROL_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                HA_CONTROL_NFC_ATTR, nfc_id_attr, false);
            esp_zb_zcl_send_update_cmd(HA_CONTROL_CLUSTER, HA_CONTROL_NFC_ATTR);
            esp_zb_lock_release();

            break;
    }
}

static esp_err_t deferred_driver_init(void) {
    static bool is_inited = false;
    if (!is_inited) {
        i2c_driver_init_master();
        lcd_driver_init();
        servo_driver_init(0);
#if CONFIG_SMART_GATE_EXIT
        rc522_driver_init((esp_nfc_callback_t) esp_app_nfc_handler, 125);
#endif

        lcd_driver_clear();

        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG,
                        "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    const uint32_t *p_sg_p = signal_struct->p_app_signal;
    const esp_err_t err_status = signal_struct->esp_err_status;
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

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                        "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
             message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);

    if (message->info.dst_endpoint == HA_ESP_ENDPOINT) {
        if (message->info.cluster == HA_CONTROL_CLUSTER) {
            if (message->attribute.id == HA_CONTROL_GATE_ATTR && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                gate_state = message->attribute.data.value ? *(bool *) message->attribute.data.value : gate_state;

                servo_driver_set_angle(gate_state ? 90 : 0);
                ESP_LOGI(TAG, "Gate set to %s", gate_state ? "On" : "Off");
            }
            else if (message->attribute.id == HA_CONTROL_DISPLAY_ATTR && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING) {
                strlcpy(display_text_attr, message->attribute.data.value, MIN(message->attribute.data.size + 1, 4 * 16));

                lcd_driver_print(display_text_attr + 1);
                ESP_LOGI(TAG, "Lcd display output set to '%s'", display_text_attr + 1);
            }
        }
    }
    return ret;
}

static esp_err_t zb_custom_cmd_handler(const esp_zb_zcl_custom_cluster_command_message_t *message)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Receive custom command: %d from address 0x%04hx", message->info.command.id, message->info.src_address.u.short_addr);
    ESP_LOGI(TAG, "Payload size: %d", message->data.size);
    ESP_LOG_BUFFER_CHAR(TAG, (uint8_t *)message->data.value + 1, MAX(message->data.size - 1, 0));

    if (message->info.dst_endpoint == HA_ESP_ENDPOINT) {
        if (message->info.cluster == HA_CONTROL_CLUSTER) {
            if (message->info.command.id == HA_CONTROL_CLEAR_DISPLAY_CMD) {
                display_text_attr[0] = 0;

                lcd_driver_clear();
                ESP_LOGI(TAG, "Cleared display");

                esp_zb_lock_acquire(portMAX_DELAY);
                esp_zb_zcl_set_attribute_val(HA_ESP_ENDPOINT,
                    HA_CONTROL_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                    HA_CONTROL_DISPLAY_ATTR, display_text_attr, false);
                esp_zb_zcl_send_update_cmd(HA_CONTROL_CLUSTER, HA_CONTROL_DISPLAY_ATTR);
                esp_zb_lock_release();
            }
#if CONFIG_SMART_GATE_EXIT
            else if (message->info.command.id == HA_CONTROL_CLEAR_NFC_CMD) {
                nfc_id_attr[0] = 0;

                ESP_LOGI(TAG, "Cleared NFC");

                esp_zb_lock_acquire(portMAX_DELAY);
                esp_zb_zcl_set_attribute_val(HA_ESP_ENDPOINT,
                    HA_CONTROL_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                    HA_CONTROL_NFC_ATTR, nfc_id_attr, false);
                esp_zb_zcl_send_update_cmd(HA_CONTROL_CLUSTER, HA_CONTROL_NFC_ATTR);
                esp_zb_lock_release();
            }
#endif
        }
    }

    return ret;
}

static esp_err_t zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);

    const esp_zb_zcl_read_attr_resp_variable_t *variable = message->variables;
    while (variable) {
        ESP_LOGI(TAG, "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)", variable->status,
                 message->info.cluster, variable->attribute.id, variable->attribute.data.type,
                 variable->attribute.data.value ? *(uint8_t *)variable->attribute.data.value : 0);
        variable = variable->next;
    }

    return ESP_OK;
}

static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message) {
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                        "Received message: error status(%d)",
                        message->info.status);

    const esp_zb_zcl_config_report_resp_variable_t *variable = message->variables;
    while (variable) {
        ESP_LOGI(TAG, "Configure report response: status(%d), cluster(0x%x), attribute(0x%x)", message->info.status,
                 message->info.cluster,
                 variable->attribute_id);
        variable = variable->next;
    }

    return ESP_OK;
}

static esp_err_t zb_action_handler(const esp_zb_core_action_callback_id_t callback_id, const void *message) {
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *) message);
            break;

        case ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID:
            ret = zb_custom_cmd_handler((esp_zb_zcl_custom_cluster_command_message_t *) message);
            break;

        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
            ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
            break;

        case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
            ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *) message);
            break;

        default:
            ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
            break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters) {
    uint8_t uint8_tmp;

    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();

    // --------------------------------- Endpoint 1 -- Basic Cluster -------------------------------------
    /* basic cluster */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);;
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER);

    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &uint8_tmp);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &uint8_tmp);

    /* identify cluster */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &uint8_tmp);

    /* control cluster */
    esp_zb_attribute_list_t *cluster = esp_zb_zcl_attr_list_create(HA_CONTROL_CLUSTER);
    esp_zb_custom_cluster_add_custom_attr(cluster, HA_CONTROL_GATE_ATTR, ESP_ZB_ZCL_ATTR_TYPE_BOOL, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &gate_state);
    esp_zb_custom_cluster_add_custom_attr(cluster, HA_CONTROL_DISPLAY_ATTR, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &display_text_attr);
#if CONFIG_SMART_GATE_EXIT
    esp_zb_custom_cluster_add_custom_attr(cluster, HA_CONTROL_NFC_ATTR, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &nfc_id_attr);
#endif

    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    const esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_ENDPOINT,
        .app_profile_id = APP_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_LEVEL_CONTROLLABLE_OUTPUT_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config);
    // --------------------------------------- End Endpoint 1 --------------------------------------------

    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
