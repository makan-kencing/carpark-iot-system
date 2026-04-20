#pragma once
#include <stdbool.h>
#include <esp_err.h>

esp_err_t led_driver_init();

esp_err_t led_driver_set_state(bool state);
