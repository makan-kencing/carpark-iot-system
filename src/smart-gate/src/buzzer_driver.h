#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t buzzer_driver_init();

esp_err_t buzzer_driver_pulse(TickType_t delay);
