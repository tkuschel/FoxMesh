/**
 * @file i2c.hpp
 * @brief Thread-safe I2C Master Bus Wrapper for FoxMesh
 * @author OM Tom (OE3TKT)
 * 
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

class i2c {
public:
    // Constructor initializes GPIO pins and configures the native ESP-IDF bus
    i2c(int sda_pin, int scl_pin, uint32_t clk_speed_hz);
    ~i2c();

    // Disable copy semantics to prevent duplicate hardware resource assignment
    i2c(const i2c&) = delete;
    i2c& operator=(const i2c&) = delete;

    /**
     * @brief Scans the network space for active I2C addresses.
     * 
     * Probes the hardware address space from 0x03 to 0x77 using low-overhead
     * commands and prints a formatted matrix directly to stdout.
     */
    void scan() const;

    // Public getter to expose the low-level bus handle to concrete peripheral drivers
    i2c_master_bus_handle_t get_bus_handle() const { return _bus_handle; }

private:
    i2c_master_bus_handle_t _bus_handle = nullptr;
};
