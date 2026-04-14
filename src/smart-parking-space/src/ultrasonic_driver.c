#include "ultrasonic_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ULTRASONIC_DRIVER";


static void ultrasonic_driver_set_baseline(ultrasonic_t *ultrasonic_arr, const size_t arr_size) {
    const int count = 5;
    int success_count = 0;

    float *total_distances = calloc(arr_size, sizeof(float));
    if (total_distances == NULL) {
        return;
    }

    float *distance_buffer = calloc(arr_size, sizeof(float));
    if (distance_buffer == NULL) {
        free(total_distances);
        return;
    }

    while (success_count < count) {
        const esp_err_t res = ultrasonic_driver_measure(ultrasonic_arr, distance_buffer, arr_size);
        if (res != ESP_OK) {
            ESP_LOGW(TAG, "Sensor: Calibration failed, retrying...");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        for (int j = 0; j < arr_size; j++) {
            if (distance_buffer[j] > 0.05f) {
                total_distances[j] += distance_buffer[j];
                ESP_LOGI(TAG, "Sensor %d: Reading %d = %.2f m", j, success_count, distance_buffer[j]);
            } else {
                ESP_LOGW(TAG, "Sensor %d: Ignoring noise %.2f m", j, distance_buffer[j]);
            }
        }
        success_count++;
    }

    for (int i = 0; i < arr_size; i++) {
        ultrasonic_arr[i].baseline_distance_cm = total_distances[i] / (float) success_count;
        ESP_LOGI(TAG, "Sensor %d: Final baseline = %.2f m", i, ultrasonic_arr[i].baseline_distance_cm);
    }

    free(total_distances);
    free(distance_buffer);
}

void ultrasonic_driver_init(ultrasonic_t *ultrasonic_arr, const size_t arr_size) {
    for (int i = 0; i < arr_size; i++) {
        const ultrasonic_sensor_t *sensor = &ultrasonic_arr[i].sensor;

        ESP_LOGI(TAG, "Initializing sensor %d, trigger pin: %d, echo pin: %d", i, sensor->trigger_pin, sensor->echo_pin);
        ESP_ERROR_CHECK(ultrasonic_init(sensor));
    }

    ESP_LOGI(TAG, "Sensor: Calibrating...");
    ultrasonic_driver_set_baseline(ultrasonic_arr, arr_size);
}

esp_err_t ultrasonic_driver_measure(const ultrasonic_t *ultrasonic_arr, float *distances, const size_t buffer_size) {
    esp_err_t res = ESP_OK;

    for (int i = 0; i < buffer_size; i++) {
        float distance;
        res = ultrasonic_measure(&ultrasonic_arr[i].sensor, 5, &distance);
        vTaskDelay(pdMS_TO_TICKS(50));

        if (res != ESP_OK) {
            switch (res) {
                case ESP_ERR_ULTRASONIC_PING:
                    ESP_LOGW(TAG, "Sensor %d: Cannot ping", i);
                    break;
                case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
                    ESP_LOGW(TAG, "Sensor %d: Ping timeout", i);
                    break;
                case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
                    ESP_LOGW(TAG, "Sensor %d: Echo timeout", i);
                    break;
                default:
                    ESP_LOGW(TAG, "Sensor %d: %s", i, esp_err_to_name(res));
            }
            return res;
        }

        distances[i] = distance;
    }

    return res;
}
