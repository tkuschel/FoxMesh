/**
 * @file i2c.cpp
 * @brief Thread-safe I2C Master Bus Implementation for FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "i2c.hpp"
#include <cstdio>
#include "esp_log.h"

static const char* TAG = "I2C";

i2c::i2c(int sda_pin, int scl_pin, uint32_t clk_speed_hz) {
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = static_cast<gpio_num_t>(sda_pin); // FIXED: Added explicit typecast to gpio_num_t
    bus_config.scl_io_num = static_cast<gpio_num_t>(scl_pin); // FIXED: Added explicit typecast to gpio_num_t
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = 1;

    // Allocate resources for the new infrastructure-driven master driver
    esp_err_t err = i2c_new_master_bus(&bus_config, &_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C Master Bus: %s", esp_err_to_name(err));
        _bus_handle = nullptr;
    } else {
        ESP_LOGI(TAG, "I2C Master Bus initialized successfully.");
    }
}

i2c::~i2c() {
    if (_bus_handle != nullptr) {
        i2c_del_master_bus(_bus_handle);
        _bus_handle = nullptr;
    }
}

void i2c::scan() const {
    if (_bus_handle == nullptr) {
        printf("Error: I2C bus handle is null. Cannot scan.\r\n");
        return;
    }

    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    printf("00:          ");

    int devices_found = 0;

    // Standard I2C address space ranges from 0x03 to 0x77
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        if (addr % 16 == 0) {
            printf("\r\n%02x:", addr);
        }

        // Use low-overhead hardware probing instead of allocating permanent device handles
        esp_err_t err = i2c_master_probe(_bus_handle, addr, 100);

        if (err == ESP_OK) {
            printf(" %02x", addr);
            devices_found++;
        } else if (err == ESP_ERR_NOT_FOUND) {
            printf(" --");
        } else {
            printf(" Er"); // Bus lockup, collision or framing timeout error occurred
        }
    }
    printf("\r\n\r\nScan finished. Found %d device(s).\r\n", devices_found);
}
