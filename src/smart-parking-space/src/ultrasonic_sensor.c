#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ultrasonic_sensor.h"

#include <esp_check.h>
#include <esp_log.h>

#define ESP_LOGW_ULTRASONIC(i, res) switch (res) { \
        case ESP_ERR_ULTRASONIC_PING: \
            ESP_LOGW(TAG, "Sensor %d: Cannot ping", i); \
            break; \
        case ESP_ERR_ULTRASONIC_PING_TIMEOUT: \
            ESP_LOGW(TAG, "Sensor %d: Ping timeout", i); \
            break; \
        case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT: \
            ESP_LOGW(TAG, "Sensor %d: Echo timeout", i); \
            break; \
        default: \
            ESP_LOGW(TAG, "Sensor %d: %s", i, esp_err_to_name(res)); \
    }

#define MAX_DISTANCE_M 5.0f
#define THRESHOLD_DELTA_M 0.05f
#define SAMPLE_COUNT 5

static const ultrasonic_sensor_config_t *global_cfg;
static ultrasonic_sensor_callback_t func_ptr;
static uint16_t interval;

static const char *TAG = "ULTRASONIC_SENSOR";


static esp_err_t ultrasonic_sensor_measure_baseline(
    const ultrasonic_sensor_t *sensor,
    const float max_distance,
    const size_t sample_count,
    float *distance
) {
    size_t success_count = 0;
    float total = 0;
    for (int i = 0; i < sample_count; i++) {
        float reading;
        const esp_err_t res = ultrasonic_measure(sensor, max_distance, &reading);

        if (res != ESP_OK) {
            ESP_LOGW_ULTRASONIC(i, res);
            continue;
        }

        if (reading < 0.05f) {
            ESP_LOGW(TAG, "Sensor %d: Ignoring noise %.2f m", i, reading);
        }

        success_count++;
        total += reading;
        ESP_LOGI(TAG, "Sensor %d: Reading %d = %.2f m", i, success_count, reading);
    }

    ESP_RETURN_ON_FALSE(success_count == 0, ESP_FAIL, TAG, "No usable reading");

    *distance = total / (float) success_count;
    return ESP_OK;
}

static esp_err_t ultrasonic_sensor_init_sensors() {
    esp_err_t res = ESP_OK;

    for (int i = 0; i < global_cfg->sensors.count; i++) {
        ultrasonic_sensor_info_t *info = &global_cfg->sensors.data[i];
        const ultrasonic_sensor_t *sensor = &info->sensor;

        ESP_LOGI(TAG, "Initializing sensor %d, trigger pin: %d, echo pin: %d",
                 i, sensor->trigger_pin, sensor->echo_pin);
        res = ultrasonic_init(sensor);
        ESP_RETURN_ON_ERROR(res, TAG, "Failed to initialize sensor %d", i);

        ESP_LOGI(TAG, "Sensor %d: Calibrating...", i);
        res = ultrasonic_sensor_measure_baseline(
            sensor,
            MAX_DISTANCE_M,
            SAMPLE_COUNT,
            &info->baseline_distance_m
        );
        ESP_RETURN_ON_ERROR(res, TAG, "Failed to measure baseline for sensor %d", i);

        ESP_LOGI(TAG, "Sensor %d: Final baseline = %.2f m", i, info->baseline_distance_m);
    }

    return res;
}


static void ultrasonic_sensor_update(void *pvParameters) {
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        int8_t delta = 0;
        for (int i = 0; i < global_cfg->sensors.count; i++) {
            float reading;

            ultrasonic_sensor_info_t *info = &global_cfg->sensors.data[i];
            const ultrasonic_sensor_t *sensor = &info->sensor;

            ultrasonic_measure(sensor, MAX_DISTANCE_M, &reading);

            ESP_LOGD(TAG, "Sensor %d: %.2f m (baseline: %.2f m, delta: %.2f m)",
                     i, reading, info->baseline_distance_m, delta);

            if (reading < info->baseline_distance_m - THRESHOLD_DELTA_M && !info->is_occupied) {
                info->is_occupied = true;
                delta--;
            } else if (reading >= info->baseline_distance_m - THRESHOLD_DELTA_M && info->is_occupied) {
                info->is_occupied = false;
                delta++;
            }
        }

        if (delta != 0 && func_ptr) {
            func_ptr(delta);
        }

        vTaskDelay(pdMS_TO_TICKS(interval * 1000));
    }
}

esp_err_t ultrasonic_sensor_init(
    const ultrasonic_sensor_config_t *cfg,
    const ultrasonic_sensor_callback_t cb,
    const uint16_t update_interval
) {
    global_cfg = cfg;
    func_ptr = cb;
    interval = update_interval;

    ESP_RETURN_ON_ERROR(ultrasonic_sensor_init_sensors(), TAG, "Failed to initialize ultrasonic sensors");

    return xTaskCreate(ultrasonic_sensor_update, "Sensor_main", 4096, NULL, 5, NULL) == pdTRUE
               ? ESP_OK
               : ESP_FAIL;
}
