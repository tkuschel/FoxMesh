/**
 * @file pmu.hpp
 * @brief Power Management Unit for the AXP2101 PMIC inside LilyGo T-Beam S3 Supreme
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

 #include "pmu.hpp"
#include "esp_log.h"

static const char* TAG = "PMU";

PowerManagement::PowerManagement(i2c_master_bus_handle_t bus_handle, uint8_t device_address)
    : _address(device_address) {

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _address,
        .scl_speed_hz = 400000, // <--- Set to 400 kHz (I2C Fast-mode)
        .scl_wait_us = 0,
        .flags = { .disable_ack_check = 0 }
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &_dev_handle));
}

esp_err_t PowerManagement::write_register(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(_dev_handle, write_buf, sizeof(write_buf), -1);
}

esp_err_t PowerManagement::init() {
    ESP_LOGI(TAG, "Initializing AXP2101 PMIC...");

    // Set ALDO3 (LoRa) to 3.3V (0x1A corresponds to 3.3V in AXP2101 step calculation)
    ESP_ERROR_CHECK(write_register(REG_ALDO3_VOLTAGE, 0x1A));

    // Set ALDO4 (u-blox M10S) to 3.3V
    ESP_ERROR_CHECK(write_register(REG_ALDO4_VOLTAGE, 0x1A));

    // Enable ALDO1, ALDO3, ALDO4 power outputs
    // Bitmask depends on PMIC routing for T-Beam Supreme
    ESP_ERROR_CHECK(write_register(REG_POWER_ENABLE2, 0x1D));

    ESP_LOGI(TAG, "PMU rails configured successfully.");
    return ESP_OK;
}
