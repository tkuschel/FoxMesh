/**
 * @file lora.hpp
 * @brief Thread-safe asynchronous Semtech SX1276 LoRa transceiver driver interface.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lora_types.hpp"
#include <iostream>
#include <string>
#include <cstring>


/**
 * @brief Controller class orchestrating hardware configurations, data transfers, and metrics collection.
 */
class LoraController {
public:

    /**
     * @brief Simple internal abstraction layer mirroring the raw incoming packet payload.
     */
    struct LoraPacket {
        float rssi;            /**< High-precision calibrated received signal power in dBm. */
        float snr;             /**< High-precision calibrated signal-to-noise ratio in dB. */
        uint8_t length;        /**< Payload size explicitly specified in bytes. */
        uint8_t payload[256];  /**< Fixed internal memory buffer capturing raw ingress bytes. Max SX1276 FIFO is 256. */
    };


    explicit LoraController();
    virtual ~LoraController();

    // Disallow copying to prevent duplicate hardware resource assignment
    LoraController(const LoraController&) = delete;
    LoraController& operator=(const LoraController&) = delete;

    /**
     * @brief Configures peripheral resources and registers the background worker thread on Core 1.
     * @param priority Scheduler task priority level execution metric.
     */
    void startTask(uint32_t priority = 6);

    /**
     * @brief Transmits a raw data frame onto the physical radio frequency layer.
     * @param buffer Pointer to the source byte array holding the packet context.
     * @param length Total number of bytes to dispatch over the air interface.
     * @return esp_err_t status indicating transaction success.
     */
    esp_err_t transmitFrame(const uint8_t* buffer, uint8_t length);

    /**
     * @brief Dynamically updates the transceiver operating frequency at runtime.
     * @param msb Most Significant Byte of the frequency fraction.
     * @param mid Middle Significant Byte of the frequency fraction.
     * @param lsb Least Significant Byte of the frequency fraction.
     */
    // void setFrequency(uint8_t msb, uint8_t mid, uint8_t lsb);

        /**
     * @brief Thread-safely updates the operational state tracking variables and overrides transceiver registers.
     * @param freq_hz Core RF targeting frequency structured in absolute Hertz scale units.
     */
    void setFrequency(uint32_t freq_hz);

    /**
     * @brief Dynamically updates the LoRa signal bandwidth at runtime.
     * @param bw_khz Target bandwidth in kHz (e.g., 62, 125, 250, 500).
     */
    void setBandwidth(uint32_t bw_khz);

    /**
     * @brief Generates an atomic, high-performance runtime snapshot of configuration parameters and metrics.
     * @details This routine performs a rapid, thread-safe memory copy of the volatile tracking state variables.
     *          Unlike getPacketListCopy(), this method explicitly excludes the heavy heap-allocated packet list data,
     *          making it exceptionally lightweight and highly suitable for high-frequency UI display refreshes,
     *          CLI updates, or periodic remote health diagnostics.
     * @note Acquires the internal synchronization mutex lock momentarily to guarantee structural telemetry alignment.
     * @return A self-contained snapshot structure mapping the current system hardware configurations and operational counts.
     */
     LoraStateSnapshot getSnapshot();

private:
    static const char* TAG;

    spi_device_handle_t m_spiHandle = nullptr;
    TaskHandle_t m_taskHandle = nullptr;
    SemaphoreHandle_t m_isrSemaphore = nullptr;

    LoraStateTable m_state;                     ///< Internal volatile tracking record data context.
    SemaphoreHandle_t m_stateMutex = nullptr;   ///< Mutex primitive enforcing mutual exclusion over m_state modifications.


    static void taskTrampoline(void* pvParameters);
    static void IRAM_ATTR gpioIsrHandler(void* arg);

    void run();
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);

    esp_err_t initializeHardware();
    void setModeReceiver();

    void applyModemConfig();

    /**
     * @brief Evaluates raw register readings into calibrated RSSI and SNR telemetry metrics.
     * @details Automatically compensates the RSSI offset base based on the active carrier frequency
     *          band (Low-Band vs High-Band boundary at 525 MHz) as well as sub-noise SNR conditions.
     * @param rawRssi The raw digital output read from RegRssiValue (0x1A).
     * @param rawSnr The raw digital twos-complement output read from RegPktSnrValue (0x1B).
     * @param[out] outRssi Reference destination pointer receiving the calibrated RSSI value in dBm.
     * @param[out] outSnr Reference destination pointer receiving the calibrated SNR value in dB.
     */
    void calculateTelemetry(uint8_t rawRssi, uint8_t rawSnr, float& outRssi, float& outSnr);

    size_t utf8_advance(const std::string& str, size_t start_byte, size_t num_chars);

};
