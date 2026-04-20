#include "driver/i2c.h"
#include "i2c_driver.h"

#include <esp_check.h>

static const char* TAG = "I2C_DRIVER";

esp_err_t i2c_driver_init_master() {
    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_I2C_MASTER_SDA_GPIO,
        .scl_io_num = CONFIG_I2C_MASTER_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_MASTER_NUM, &conf), TAG, );
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_TX_BUF_DISABLE, I2C_MASTER_RX_BUF_DISABLE, 0), TAG, );

    return ESP_OK;
}
