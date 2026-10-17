/**
 * @file display.hpp
 * @brief Thread-safe asynchronous SSD1306 OLED display controller for FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "i2c.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class DisplayController {
public:
    explicit DisplayController(i2c* bus_pointer);
    virtual ~DisplayController();

    // Disallow copying to prevent duplicate hardware resource assignment
    DisplayController(const DisplayController&) = delete;
    DisplayController& operator=(const DisplayController&) = delete;

    /**
     * @brief Spawns the internal FreeRTOS queue consumer task on Core 0.
     * @param priority Scheduler task priority level.
     */
    void startTask(uint32_t priority = 3);

    /**
     * @brief Thread-safe non-blocking method to print a message to the OLED display.
     * @param text Null-terminated string message (maximum 20 characters for simple lines).
     * @param line Destination line identifier index (0 to 7 for page addressing mode).
     */
    void printMessage(const char* text, uint8_t line);

private:
    // Internal structure mapping the safe asynchronous data pipeline
    struct DisplayMessage {
        char content[24];
        uint8_t targetLine;
    };

    i2c* m_bus;
    i2c_master_dev_handle_t m_devHandle = nullptr;
    QueueHandle_t m_msgQueue = nullptr;

    static void taskTrampoline(void* pvParameters);
    void run();

    // Hardware-level low-overhead command layer execution abstraction
    esp_err_t sendCommand(uint8_t cmd) const;
    esp_err_t initializeHardware();
};
