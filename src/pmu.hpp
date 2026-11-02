/**
 * @file pmu.hpp
 * @brief Power Management Unit for the AXP2101 PMIC inside LilyGo T-Beam S3 Supreme
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

class PowerManagement {
public:
    PowerManagement(i2c_master_bus_handle_t bus_handle, uint8_t device_address);
    ~PowerManagement() = default;

    esp_err_t init();
    esp_err_t enable_lora_power();
    esp_err_t enable_gps_power();
    esp_err_t enable_peripherals_power();

private:
    esp_err_t write_register(uint8_t reg, uint8_t data);

    i2c_master_dev_handle_t _dev_handle{nullptr};
    uint8_t _address;

    // AXP2101 Register Definitions
    static constexpr uint8_t REG_POWER_ENABLE2 = 0x90;
    static constexpr uint8_t REG_BLD0x_VOLTAGE = 0x91;
    static constexpr uint8_t REG_DLD0x_VOLTAGE = 0x92;

    static constexpr uint8_t REG_ALDO3_VOLTAGE = 0x94;
    static constexpr uint8_t REG_ALDO4_VOLTAGE = 0x95;
};
