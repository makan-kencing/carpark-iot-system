#pragma once

#include "ultrasonic.h"


typedef struct {
    const ultrasonic_sensor_t sensor;
    bool is_occupied;
    float baseline_distance_m;
} ultrasonic_sensor_info_t;

typedef struct {
    struct {
        ultrasonic_sensor_info_t *data;
        const size_t count;
    } sensors;
} ultrasonic_sensor_config_t;

typedef void (*ultrasonic_sensor_callback_t)(int8_t delta);

esp_err_t ultrasonic_sensor_init(const ultrasonic_sensor_config_t *cfg,
                                 ultrasonic_sensor_callback_t cb,
                                 uint16_t update_interval);
