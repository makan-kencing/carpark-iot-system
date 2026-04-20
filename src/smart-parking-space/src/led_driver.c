#include "led_driver.h"

#include <esp_check.h>
#include <esp_log.h>
#include <driver/gpio.h>

static const char *TAG = "LED_DRIVER";

void led_driver_init() {
    ESP_LOGI(TAG, "Setting up Red LED at %d and Green LED at %d", CONFIG_RED_LED_GPIO, CONFIG_GREEN_LED_GPIO);
    ESP_ERROR_CHECK(gpio_reset_pin(CONFIG_RED_LED_GPIO));
    ESP_ERROR_CHECK(gpio_reset_pin(CONFIG_GREEN_LED_GPIO));
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_RED_LED_GPIO, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_GREEN_LED_GPIO, GPIO_MODE_OUTPUT));

    ESP_ERROR_CHECK(gpio_set_level(CONFIG_RED_LED_GPIO, 0));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_GREEN_LED_GPIO, 1));
}

esp_err_t led_driver_set_state(const bool state) {
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_GREEN_LED_GPIO, state), TAG, );
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_RED_LED_GPIO, !state), TAG, );

    return ESP_OK;
}