#pragma once

#include <math.h>

/* Servo Gate Configuration */
#define SERVO_GPIO               8     /* ESP32-C6 safe GPIO */
#define SERVO_MIN_PULSEWIDTH_US  500   /* 0 degrees pulse width */
#define SERVO_MAX_PULSEWIDTH_US  2500  /* 180 degrees pulse width */
#define SERVO_MAX_DEGREE         180   /* Max physical angle */
#define SERVO_PERIOD_US          20000 /* 50Hz frequency (20ms) */

#define LEDC_TIMER               LEDC_TIMER_0
#define LEDC_MODE                LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL             LEDC_CHANNEL_0
#define LEDC_DUTY_RES            LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY           50

void servo_driver_set_angle(uint32_t angle);

void servo_driver_init(uint32_t default_angle);
