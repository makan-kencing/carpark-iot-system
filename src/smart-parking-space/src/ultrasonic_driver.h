#pragma once

#include <stdbool.h>
#include "ultrasonic.h"

typedef struct {
    ultrasonic_sensor_t sensor;
    bool is_occupied;
    float baseline_distance_cm;
} ultrasonic_t;

void ultrasonic_driver_init(ultrasonic_t *ultrasonic_arr, size_t arr_size);
esp_err_t ultrasonic_driver_measure(const ultrasonic_t *ultrasonic_arr, float *distances, size_t buffer_size);


