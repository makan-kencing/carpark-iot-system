#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c_driver.h"
#include "lcd_driver.h"

#include <esp_check.h>

static const char* TAG = "LCD_DRIVER";

static esp_err_t lcd_driver_send_cmd(const char cmd) {
    const char data_u = cmd & 0xf0;
    const char data_l = cmd << 4 & 0xf0;
    const uint8_t data_t[4] = { data_u | 0x0C, data_u | 0x08, data_l | 0x0C, data_l | 0x08 };
    const esp_err_t res = i2c_master_write_to_device(I2C_MASTER_NUM, CONFIG_LCD_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
    vTaskDelay(pdMS_TO_TICKS(2));
    return res;
}

static esp_err_t lcd_driver_send_char(const char c) {
    const char data_u = c & 0xf0;
    const char data_l = c << 4 & 0xf0;
    const uint8_t data_t[4] = { data_u | 0x0D, data_u | 0x09, data_l | 0x0D, data_l | 0x09 };
    const esp_err_t res = i2c_master_write_to_device(I2C_MASTER_NUM, CONFIG_LCD_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
    vTaskDelay(pdMS_TO_TICKS(2)); // give lcd time to print
    return res;
}

esp_err_t lcd_driver_set_cursor(const int row, const int col) {
    // 1000 0000  <- write cmd
    //  1         <- row part
    //         1  <- col part
    return lcd_driver_send_cmd(LCD_CMD_WRITE | row << 6 | col);
}

esp_err_t lcd_driver_clear() {
    const esp_err_t res = lcd_driver_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Clear display command
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for the command to execute
    return res;
}

esp_err_t lcd_driver_print(const char *str) {
    ESP_RETURN_ON_ERROR(lcd_driver_clear(), TAG, );

    int row = 0;
    char current;
    while ((current = *str++)) {
        if (current == '\n') {
            row++;
            ESP_RETURN_ON_ERROR(lcd_driver_set_cursor(row, 0), TAG, );
            continue;
        }

        ESP_RETURN_ON_ERROR(lcd_driver_send_char(current), TAG, );
    }

    return ESP_OK;
}

esp_err_t lcd_driver_init() {
    // 4 bit initialization sequence
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_INIT_8_BIT_MODE), TAG, );
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_INIT_8_BIT_MODE), TAG, );
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_INIT_8_BIT_MODE), TAG, );
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_INIT_4_BIT_MODE), TAG, ); // Set 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(1));

    // display initialization
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_FUNCTION_SET(0, 1, 0)), TAG, ); // Function set: 4-bit mode, 2-line display, 5x8 characters
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_DISPLAY_OFF), TAG, ); // Display off
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_CLEAR_DISPLAY), TAG, ); // Clear display
    vTaskDelay(pdMS_TO_TICKS(2));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_ENTRY_MODE_SET), TAG, ); // Entry mode set: increment cursor, no shift
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(lcd_driver_send_cmd(LCD_CMD_DISPLAY_ON), TAG, ); // Display on, cursor off, blink off
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}
