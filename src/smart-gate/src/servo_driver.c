#include "sys/param.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_check.h"

#include "servo_driver.h"

const char *TAG = "SERVO_DRIVER";

static uint32_t servo_angle_to_duty(const uint32_t angle) {
    const uint32_t pulse_width = SERVO_MIN_PULSEWIDTH_US +
                                 (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle / SERVO_MAX_DEGREE;
    return pulse_width * 8191 / SERVO_PERIOD_US;
}

esp_err_t servo_driver_set_angle(uint32_t angle) {
    angle = MIN(angle, SERVO_MAX_DEGREE);

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, servo_angle_to_duty(angle)), TAG, );
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG, );

    return ESP_OK;
}

esp_err_t servo_driver_init(uint32_t default_angle) {
    default_angle = MIN(default_angle, SERVO_MAX_DEGREE);

    const ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = CONFIG_SERVO_GPIO,
        .duty = servo_angle_to_duty(default_angle),
        .hpoint = 0
    };
    return ledc_channel_config(&ledc_channel);
}
