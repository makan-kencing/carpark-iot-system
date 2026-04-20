#include "buzzer_driver.h"

#include "esp_check.h"
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "BUZZER_DRIVER";

#ifdef CONFIG_SMART_GATE_EXIT
esp_err_t buzzer_driver_init() {
    // Configure the LEDC timer for the PWM signal
    const ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_13_BIT, // 13-bit resolution (0-8192)
        .freq_hz = 2000, // 2 kHz frequency
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, );

    // Configure the LEDC channel to attach to the GPIO pin
    const ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_1,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = CONFIG_BUZZER_GPIO,
        .duty = 0, // Start OFF
        .hpoint = 0
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ledc_channel), TAG, );

    return ESP_OK;
}

esp_err_t buzzer_driver_pulse(const TickType_t delay) {
    // Set duty cycle to 50% (4096 out of 8192) to turn the buzzer ON
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 4096), TAG,);
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1), TAG,);
    vTaskDelay(delay);
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0), TAG,);
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1), TAG,);

    return ESP_OK;
}
#endif
