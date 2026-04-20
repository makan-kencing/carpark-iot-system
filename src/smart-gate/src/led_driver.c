#include "led_driver.h"

#include <esp_check.h>
#include <driver/gpio.h>

static const char* TAG = "LED_DRIVER";

esp_err_t led_driver_init() {
    ESP_RETURN_ON_ERROR(gpio_reset_pin(CONFIG_LED_GREEN_GPIO), TAG, );
    ESP_RETURN_ON_ERROR(gpio_reset_pin(CONFIG_LED_RED_GPIO), TAG, );

    ESP_RETURN_ON_ERROR(gpio_set_direction(CONFIG_LED_GREEN_GPIO, GPIO_MODE_OUTPUT), TAG, );
    ESP_RETURN_ON_ERROR(gpio_set_direction(CONFIG_LED_RED_GPIO, GPIO_MODE_OUTPUT), TAG, );

    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LED_GREEN_GPIO, 0), TAG, );
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LED_RED_GPIO, 1), TAG, );

    return ESP_OK;
}

esp_err_t led_driver_set_state(const bool state) {
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LED_RED_GPIO, !state), TAG, );
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LED_GREEN_GPIO, state), TAG, );

    return ESP_OK;
}
