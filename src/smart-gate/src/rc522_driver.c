#include "rc522_driver.h"

#include <esp_check.h>
#include <esp_log.h>

#include "rc522.h"
#include "rc522_picc.h"
#include "driver/rc522_spi.h"
#include "picc/rc522_nxp.h"

#define BUFFER_SIZE 128

#ifdef CONFIG_SMART_GATE_EXIT
static const char *TAG = "RC522_DRIVER";

static esp_nfc_callback_t func_ptr;
static rc522_driver_handle_t driver;
static rc522_handle_t scanner;

static rc522_spi_config_t driver_config = {
    .host_id = SPI2_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = CONFIG_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_SPI_SCLK_GPIO,
    },
    .dev_config = {
        .spics_io_num = CONFIG_RC522_SPICS_GPIO,
        .clock_speed_hz = 10000,  // 10khz
    },
    .rst_io_num = CONFIG_SPI_RST_GPIO,
};

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    ESP_LOGI(TAG, "NFC detected");

    const rc522_picc_state_changed_event_t *event = data;
    rc522_picc_t *picc = event->picc;

    rc522_picc_print(picc);

    if (picc->state != RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Reading card");
        rc522_nxp_get_type(scanner, picc, &picc->type);

        func_ptr(ESP_NFC_READ, &(esp_nfc_callback_message_read_t){
                     .picc = picc
                 });
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Removed card");
        func_ptr(ESP_NFC_REMOVE, &(esp_nfc_callback_message_remove_t){
                     .picc = picc
                 });
    }
}

esp_err_t rc522_driver_init(const esp_nfc_callback_t cb, const uint16_t poll_interval_ms) {
    func_ptr = cb;

    ESP_RETURN_ON_ERROR(rc522_spi_create(&driver_config, &driver), TAG, );
    ESP_RETURN_ON_ERROR(rc522_driver_install(driver), TAG, );

    const rc522_config_t scanner_config = {
        .driver = driver,
        .poll_interval_ms = poll_interval_ms,
    };

    ESP_RETURN_ON_ERROR(rc522_create(&scanner_config, &scanner), TAG, );
    ESP_RETURN_ON_ERROR(rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL), TAG, );
    ESP_RETURN_ON_ERROR(rc522_start(scanner), TAG, );

    return ESP_OK;
}
#endif
