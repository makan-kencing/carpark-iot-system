#pragma once
#include "esp_err.h"

/* I2C Configuration */
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          50000 /* I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0 /* I2C master doesn't need buffer for transmission */
#define I2C_MASTER_RX_BUF_DISABLE   0 /* I2C master doesn't need buffer for reception */

esp_err_t i2c_driver_init_master();