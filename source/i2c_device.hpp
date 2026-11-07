/**
 * @file i2c_device.hpp
 * @brief Generische I2C-Peripheriegerät-Basisklasse für FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_err.h"

class i2c_device {
public:
    /**
     * @brief Constructor automatically registers the device to the provided I2C bus.
     * @param bus_handle Handle of the initialized I2C master bus instance.
     * @param dev_addr The 7-bit I2C address of the peripheral device.
     * @param clk_speed_hz The desired clock frequency for this device in Hz (Default: 100kHz).
     */
    i2c_device(i2c_master_bus_handle_t bus_handle, uint16_t dev_addr, uint32_t clk_speed_hz = 100000) {
        i2c_device_config_t dev_config = {};
        dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_config.device_address = dev_addr;
        dev_config.scl_speed_hz = clk_speed_hz;

        // Register this specific device to the master bus
        esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_config, &_dev_handle);
        if (err != ESP_OK) {
            _dev_handle = nullptr;
        }
    }

    /**
     * @brief Virtual destructor ensures proper removal of the device from the bus.
     */
    virtual ~i2c_device() {
        if (_dev_handle != nullptr) {
            i2c_master_bus_rm_device(_dev_handle);
            _dev_handle = nullptr;
        }
    }

    // Disable copy semantics to prevent duplicate hardware resource assignment
    i2c_device(const i2c_device&) = delete;
    i2c_device& operator=(const i2c_device&) = delete;

    /**
     * @brief Checks if the device was successfully registered and is ready for use.
     * @return true if the device handle is valid, false otherwise.
     */
    bool is_ready() const { return _dev_handle != nullptr; }

protected:
    i2c_master_dev_handle_t _dev_handle = nullptr; ///< Internal ESP-IDF device handle

    /**
     * @brief Helper function to write data to a specific device register.
     * @param reg The target register address.
     * @param data Pointer to the data buffer to be written.
     * @param len Number of bytes to write.
     * @return esp_err_t ESP_OK on success, or appropriate error code.
     */
/*
    esp_err_t write_reg(uint8_t reg, const uint8_t* data, size_t len) {
        if (!_dev_handle) return ESP_ERR_INVALID_STATE;

        // Dynamically allocate vector to safely hold register address + data payload
        std::vector<uint8_t> buffer(len + 1);
        buffer[0] = reg; // First byte is always the target register pointer
        for (size_t i = 0; i < len; i++) {
            buffer[i + 1] = data[i];
        }

        // Transmit data over the bus
        return i2c_master_transmit(_dev_handle, buffer.data(), buffer.size(), pdMS_TO_TICKS(100));
    }
*/
    esp_err_t write_reg(uint8_t reg, const uint8_t* data, size_t len) {
        if (!_dev_handle) return ESP_ERR_INVALID_STATE;

        // VLA (Variable Length Array) on stack instead of dynamic heap allocation via vector
        uint8_t buffer[len + 1];
        buffer[0] = reg;
        for (size_t i = 0; i < len; i++) {
            buffer[i + 1] = data[i];
        }

        return i2c_master_transmit(_dev_handle, buffer, len + 1, pdMS_TO_TICKS(100));
    }

    /**
     * @brief Helper function to read data from a specific device register.
     *        (Enforces the strict STOP-START sequence required by PMUs)
     * @param reg The target register address to read from.
     * @param out_data Pointer to the destination buffer where received data will be stored.
     * @param len Number of bytes to read.
     * @return esp_err_t ESP_OK on success, or appropriate error code.
     */
    esp_err_t read_reg(uint8_t reg, uint8_t* out_data, size_t len) {
        if (!_dev_handle) return ESP_ERR_INVALID_STATE;

        // 1. Pack register address into a DMA- and word-aligned vector
        //std::vector<uint8_t> tx_buf = { reg };

        // Step A: Transmit the register pointer (concludes transaction with a STOP condition)
        //esp_err_t err = i2c_master_transmit(_dev_handle, tx_buf.data(), tx_buf.size(), pdMS_TO_TICKS(100));
        esp_err_t err = i2c_master_transmit(_dev_handle, &reg, 1, pdMS_TO_TICKS(100));
        if (err != ESP_OK) return err;

        // 2. Prepare the receive buffer as a DMA-safe vector
        //std::vector<uint8_t> rx_buf(len, 0);

        // Step B: Fetch data bytes (starts a clean new START condition on the bus)
        //err = i2c_master_receive(_dev_handle, rx_buf.data(), rx_buf.size(), pdMS_TO_TICKS(100));
        //if (err == ESP_OK) {
            // Copy safely fetched hardware data back into user buffer
        //    for (size_t i = 0; i < len; i++) {
        //        out_data[i] = rx_buf[i];
        //    }
        //}
        //return err;
               // Step B: Fetch data bytes directly into user-provided pointer buffer (clean new START condition)
        return i2c_master_receive(_dev_handle, out_data, len, pdMS_TO_TICKS(100));
    }
};
