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
#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include <string.h>

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

static uint32_t servo_angle_to_duty(uint32_t angle) {
    if (angle > SERVO_MAX_DEGREE) {
        angle = SERVO_MAX_DEGREE;
    }
    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH_US +
        ((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) / SERVO_MAX_DEGREE;
    return (pulse_width * 8191) / SERVO_PERIOD_US;
}

void init_servo(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_GPIO,
        .duty           = servo_angle_to_duty(GATE_CLOSED_ANGLE), // Gate starts closed
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void open_gate(void) {
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, servo_angle_to_duty(GATE_OPEN_ANGLE)));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void close_gate(void) {
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, servo_angle_to_duty(GATE_CLOSED_ANGLE)));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_TX_BUF_DISABLE, I2C_MASTER_RX_BUF_DISABLE, 0);
}

void lcd_send_cmd(char cmd) {
    char data_u = (cmd & 0xf0), data_l = ((cmd << 4) & 0xf0);
    uint8_t data_t[4] = { data_u | 0x0C, data_u | 0x08, data_l | 0x0C, data_l | 0x08 };
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_send_data(char data) {
    char data_u = (data & 0xf0), data_l = ((data << 4) & 0xf0);
    uint8_t data_t[4] = { data_u | 0x0D, data_u | 0x09, data_l | 0x0D, data_l | 0x09 };
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
    vTaskDelay(pdMS_TO_TICKS(2)); // give lcd time to print
}

void lcd_clear(void) {
    lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Clear display command
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for the command to execute
}

void lcd_set_cursor(int row, int col) {
    if (row == 0) lcd_send_cmd(0x80 | col);
    if (row == 1) lcd_send_cmd(0xC0 | col);
}

void lcd_send_string(const char *str) {
    while (*str) lcd_send_data(*str++); // Send each character of the string
}

void init_lcd(void) {
    i2c_master_init();

    // 4 bit initialization sequence
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_cmd(LCD_CMD_INIT_4_BIT_MODE); // Set 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(1));

    // display initialization
    lcd_send_cmd(LCD_CMD_FUNCTION_SET); // Function set: 4-bit mode, 2-line display, 5x8 characters
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_cmd(LCD_CMD_DISPLAY_OFF); // Display off
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Clear display
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_cmd(LCD_CMD_ENTRY_MODE_SET); // Entry mode set: increment cursor, no shift
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_cmd(LCD_CMD_DISPLAY_ON); // Display on, cursor off, blink off
    vTaskDelay(pdMS_TO_TICKS(1));
}

void display_welcome_message(const char* plate) {
    lcd_clear();
    char buffer[17];

    lcd_set_cursor(0, 4);
    lcd_send_string("WELCOME");
    snprintf(buffer, sizeof(buffer), "PLATE:%s", plate);

    // Auto center the bottom string
    int len = strlen(buffer);
    int offset = (16 - len) / 2;
    if (offset < 0) offset = 0;

    lcd_set_cursor(1, offset);
    lcd_send_string(buffer);
}

void display_payment_message(const char* plate, float price) {
    lcd_clear();
    char buffer[17];
    // top
    snprintf(buffer, sizeof(buffer), "CAR: %s", plate);
    lcd_set_cursor(0, 0);
    lcd_send_string(buffer);
    // bottom
    snprintf(buffer, sizeof(buffer), "TOTAL: RM%.2f", price);
    lcd_set_cursor(1, 0);
    lcd_send_string(buffer);
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(5000)); // Give PlatformIO time to open the serial monitor

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    init_servo();
    init_lcd();

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

    // Test
    while(1) {
        ESP_LOGI(TAG, "Car Arriving...");
        display_welcome_message("VBC12345");
        open_gate();
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Gate Closing...");
        lcd_clear();
        close_gate();
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Car Leaving (Payment)...");
        display_payment_message("VBC12345", 15.50);
        open_gate();
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Gate Closing...");
        lcd_clear();
        close_gate();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
