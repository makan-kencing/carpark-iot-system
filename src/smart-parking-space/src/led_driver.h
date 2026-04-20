#pragma once
#include "esp_err.h"
#include "stdbool.h"

void led_driver_init();

esp_err_t led_driver_set_state(bool state);
