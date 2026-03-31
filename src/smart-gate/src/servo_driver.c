#include "servo_driver.h"

#include <sys/param.h>

#include "driver/ledc.h"

#include "esp_log.h"

static  uint32_t servo_angle_to_duty(const uint32_t angle) {
    const uint32_t pulse_width = SERVO_MIN_PULSEWIDTH_US +
        (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle / SERVO_MAX_DEGREE;
    return pulse_width * 8191 / SERVO_PERIOD_US;
}

void servo_driver_set_angle(uint32_t angle) {
    angle = MIN(angle, SERVO_MAX_DEGREE);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, servo_angle_to_duty(angle)));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void servo_driver_init(uint32_t default_angle) {
    default_angle = MIN(default_angle, SERVO_MAX_DEGREE);

    const ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    const ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = CONFIG_SERVO_GPIO,
        .duty           = servo_angle_to_duty(default_angle),
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}


