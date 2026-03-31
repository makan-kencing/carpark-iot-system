#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c_driver.h"
#include "lcd_driver.h"

static void lcd_driver_send_cmd(const char cmd) {
    const char data_u = cmd & 0xf0;
    const char data_l = cmd << 4 & 0xf0;
    const uint8_t data_t[4] = {
        data_u | LCD_CMD_DISPLAY(1, 0, 0),
        data_u | LCD_CMD_DISPLAY(0, 0, 0),
        data_l | LCD_CMD_DISPLAY(1, 0, 0),
        data_l | LCD_CMD_DISPLAY(0, 0, 0)
    };
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void lcd_driver_send_char(const char c) {
    const char data_u = c & 0xf0;
    const char data_l = c << 4 & 0xf0;
    const uint8_t data_t[4] = {
        data_u | LCD_CMD_DISPLAY(1, 0, 1),
        data_u | LCD_CMD_DISPLAY(0, 0, 1),
        data_l | LCD_CMD_DISPLAY(1, 0, 1),
        data_l | LCD_CMD_DISPLAY(0, 0, 1)
    };
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS));
    vTaskDelay(pdMS_TO_TICKS(2)); // give lcd time to print
}

void lcd_driver_set_cursor(const int row, const int col) {
    // 1000 0000  <- write cmd
    //  1         <- row part
    //         1  <- col part
    lcd_driver_send_cmd(LCD_CMD_WRITE | row << 6 | col);
}

void lcd_driver_clear(void) {
    lcd_driver_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Clear display command
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for the command to execute
}

void lcd_driver_print(const char *str) {
    lcd_driver_clear();

    int row = 0;
    while (*str) {
        if (*str == '\n') {
            row++;
            lcd_driver_set_cursor(row, 0);
        }

        lcd_driver_send_char(*str++);
    }
}

void lcd_driver_init(void) {
    i2c_driver_init_master();

    // 4 bit initialization sequence
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_driver_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_driver_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_driver_send_cmd(LCD_CMD_INIT_8_BIT_MODE);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_driver_send_cmd(LCD_CMD_INIT_4_BIT_MODE); // Set 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(1));

    // display initialization
    lcd_driver_send_cmd(LCD_CMD_FUNCTION_SET(0, 1, 0)); // Function set: 4-bit mode, 2-line display, 5x8 characters
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_driver_send_cmd(LCD_CMD_DISPLAY_OFF); // Display off
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_driver_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Clear display
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_driver_send_cmd(LCD_CMD_ENTRY_MODE_SET); // Entry mode set: increment cursor, no shift
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_driver_send_cmd(LCD_CMD_DISPLAY_ON); // Display on, cursor off, blink off
    vTaskDelay(pdMS_TO_TICKS(1));
}
