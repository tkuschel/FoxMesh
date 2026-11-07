/**
 * @file axp2101.hpp
 * @brief AXP2101 PMIC Driver for FoxMesh implementing the i2c_device base class
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 */

#pragma once

#include "i2c_device.hpp"
#include "esp_log.h"

class axp2101 : public i2c_device {
public:
    static constexpr uint16_t PMU_ADDRESS = 0x34; ///< Default AXP2101 I2C Address

    // Verified Register Map according to AXP2101 Datasheet
    static constexpr uint8_t REG_STATUS1       = 0x00; ///< PMU Status Register 1 (Bit 3 = Battery Presence)
    static constexpr uint8_t REG_ADC_CHANNEL   = 0x30; ///< ADC Channel Enable Control (Required for telemetry)
    static constexpr uint8_t REG_BAT_VOLTAGE_H = 0x34; ///< Battery Voltage High Byte
    static constexpr uint8_t REG_BAT_VOLTAGE_L = 0x35; ///< Battery Voltage Low Byte
    static constexpr uint8_t REG_LDO_ENABLE    = 0x90; ///< ALDO/BLDO/DLDO Switch Control
    static constexpr uint8_t REG_ALDO4_VOLTAGE = 0x95; ///< ALDO4 Output Voltage Setting

    /**
     * @brief Constructor attaches the PMIC to a specific I2C bus handle.
     */
    axp2101(i2c_master_bus_handle_t bus_handle)
        : i2c_device(bus_handle, PMU_ADDRESS, 400000) {}

    /**
     * @brief Initializes the PMIC hardware channels after registration.
     *        Must be called after a short delay in app_main!
     * @return esp_err_t ESP_OK on successful ADC activation.
     */
    esp_err_t begin() {
        if (!is_ready()) return ESP_ERR_INVALID_STATE;

        // Value 0x3F enables all relevant internal ADCs (voltage, current, temperature)
        uint8_t adc_enable_value = 0x3F;
        esp_err_t err = write_reg(REG_ADC_CHANNEL, &adc_enable_value, 1);
        if (err != ESP_OK) {
            ESP_LOGE("AXP2101", "Failed to enable PMIC ADCs: %s", esp_err_to_name(err));
            return err;
        }

        ESP_LOGI("AXP2101", "PMIC Telemetry ADCs enabled successfully.");
        return ESP_OK;
    }

    /**
     * @brief Controls the power supply for the GPS module (ALDO4 Rail).
     * @param enable True to power ON (3.3V), false to power OFF (0V).
     * @return esp_err_t ESP_OK on success.
     */
    esp_err_t set_gps_power(bool enable) {
        if (!is_ready()) return ESP_ERR_INVALID_STATE;

        if (enable) {
            // 1. Set ALDO4 Voltage to 3.3V
            // Formula in Datasheet: Vout = 500mV + (reg_value * 100mV)
            // For 3300mV: (3300 - 500) / 100 = 28 (0x1C)
            uint8_t voltage_val = 0x1C;
            esp_err_t err = write_reg(REG_ALDO4_VOLTAGE, &voltage_val, 1);
            if (err != ESP_OK) return err;

            // 2. Turn ON ALDO4 Switch
            // Read current register state first to prevent changing other LDOs
            uint8_t ldo_state = 0;
            err = read_reg(REG_LDO_ENABLE, &ldo_state, 1);
            if (err != ESP_OK) return err;

            ldo_state |= (1 << 3); // Bit 3 controls ALDO4 in Register 0x90
            err = write_reg(REG_LDO_ENABLE, &ldo_state, 1);
            if (err != ESP_OK) return err;

            ESP_LOGI("AXP2101", "GPS Power Rail (ALDO4 @ 3.3V) enabled.");
        } else {
            // Turn OFF ALDO4 Switch
            uint8_t ldo_state = 0;
            esp_err_t err = read_reg(REG_LDO_ENABLE, &ldo_state, 1);
            if (err != ESP_OK) return err;

            ldo_state &= ~(1 << 3); // Clear Bit 3 to disable ALDO4
            err = write_reg(REG_LDO_ENABLE, &ldo_state, 1);
            if (err != ESP_OK) return err;

            ESP_LOGI("AXP2101", "GPS Power Rail (ALDO4) disabled.");
        }
        return ESP_OK;
    }

    /**
     * @brief Checks if a battery is physically connected.
     * @param[out] connected Set to true if a battery is detected.
     * @return esp_err_t ESP_OK on successful communication.
     */
    esp_err_t is_battery_connected(bool &connected) {
        _status_buffer = 0; // Reset persistent member buffer
        esp_err_t err = read_reg(REG_STATUS1, &_status_buffer, 1);
        if (err != ESP_OK) return err;

        // Bit 3 indicates battery presence state (1 = connected, 0 = absent)
        connected = (_status_buffer & (1 << 3));
        return ESP_OK;
    }

    /**
     * @brief Reads the current battery voltage in millivolts (mV).
     * @param[out] voltage_mv Measured voltage in mV (e.g., 4200 for 4.2V).
     * @return esp_err_t ESP_OK on successful communication.
     */
    esp_err_t get_battery_voltage(uint16_t &voltage_mv) {
        _telemetry_buffer[0] = 0;
        _telemetry_buffer[1] = 0;

        // Read 2 Bytes starting from 0x34 (High Byte first)
        esp_err_t err = read_reg(REG_BAT_VOLTAGE_H, _telemetry_buffer, 2);
        if (err != ESP_OK) return err;

        // AXP2101 Battery Voltage ADC is a 14-bit value packed into 2 bytes
        uint16_t raw_adc = ((_telemetry_buffer[0] & 0x3F) << 8) | _telemetry_buffer[1];

        // Factory calibration step: 1 LSB = 1.0 mV
        voltage_mv = raw_adc;
        return ESP_OK;
    }

private:
    // Persistent member buffers to satisfy internal ESP-IDF memory requirements
    uint8_t _status_buffer = 0;
    uint8_t _telemetry_buffer[2] = {0, 0};
};
