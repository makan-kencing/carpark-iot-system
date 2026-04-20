#include "led_driver.h"

#include <driver/gpio.h>


void led_driver_init() {
    gpio_reset_pin(CONFIG_LED_GREEN_GPIO);
    gpio_reset_pin(CONFIG_LED_RED_GPIO);

    gpio_set_direction(CONFIG_LED_GREEN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_LED_RED_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(CONFIG_LED_GREEN_GPIO, 0);
    gpio_set_level(CONFIG_LED_RED_GPIO, 1);
}

void led_driver_set_state(const bool state) {
    gpio_set_level(CONFIG_LED_RED_GPIO, !state);
    gpio_set_level(CONFIG_LED_GREEN_GPIO, state);
}
