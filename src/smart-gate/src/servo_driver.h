#pragma once
#include "esp_err.h"
#include "stdint.h"

#define SERVO_MIN_PULSEWIDTH_US  500   /* 0 degrees pulse width */
#define SERVO_MAX_PULSEWIDTH_US  2500  /* 180 degrees pulse width */
#define SERVO_MAX_DEGREE         180   /* Max physical angle */
#define SERVO_PERIOD_US          20000 /* 50Hz frequency (20ms) */

esp_err_t servo_driver_set_angle(uint32_t angle);

esp_err_t servo_driver_init(uint32_t default_angle);
