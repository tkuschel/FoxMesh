/**
 * @file lora.hpp
 * @brief Thread-safe asynchronous Semtech SX1276 LoRa transceiver driver for FoxMesh
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

class LoraController {
public:
    /**
     * @brief Structure defining the localized packet buffer space.
     */
    struct LoraPacket {
        uint8_t payload[256]; ///< Raw data payload frame matching max SX1276 FIFO allocation
        uint8_t length;       ///< Total number of active bytes captured inside the frame
        int8_t rssi;          ///< Received Signal Strength Indicator metric in dBm
        int8_t snr;           ///< Signal-to-Noise Ratio metric in dB
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

private:
    spi_device_handle_t m_spiHandle = nullptr;
    TaskHandle_t m_taskHandle = nullptr;
    SemaphoreHandle_t m_isrSemaphore = nullptr;

    static const char* TAG;

    static void taskTrampoline(void* pvParameters);
    static void IRAM_ATTR gpioIsrHandler(void* arg);

    void run();
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);

    esp_err_t initializeHardware();
    void setModeReceiver();
};
