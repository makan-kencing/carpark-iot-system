#include "buzzer_driver.h"

#include "esp_check.h"
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const char *TAG = "BUZZER_DRIVER";

esp_err_t buzzer_driver_init() {
    // Configure the LEDC channel to attach to the GPIO pin
    const ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = CONFIG_BUZZER_GPIO,
        .duty = 0, // Start OFF
        .hpoint = 0
    };
    return ledc_channel_config(&ledc_channel);
}

esp_err_t buzzer_driver_pulse() {
    // Set duty cycle to 50% (4096 out of 8192) to turn the buzzer ON
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4096), TAG,);
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG,);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0), TAG,);
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG,);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4096), TAG,);
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG,);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Set duty cycle back to 0% to turn the buzzer OFF
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0), TAG,);
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG,);

    return ESP_OK;
}
