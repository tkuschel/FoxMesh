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
     * @param line Destination line identifier index (0 to 7 for page addressing mode).
     * @param text Null-terminated string message (maximum 20 characters for simple lines).
     * @param startX Starting column coordinate index on the horizontal axis (0 to 127). Defaults to 0.
     */
    void printMessage(uint8_t line, const char* text, uint8_t startX = 0);
/**
     * @brief Appends text to a line starting at a specific X coordinate WITHOUT clearing the rest of the row.
     *        Perfect for adding dynamic values next to static labels.
     * @param line Selected hardware page index mapping from 0 to 7.
     * @param text Pointer to a standard null-terminated sequence of tracking information.
     * @param startX Starting column coordinate index on the horizontal axis (0 to 127).
     */
    inline void appendMessage(uint8_t line, const char* text, uint8_t startX) {
        // Automatically injects the 0x80 bit to tell the consumer task to disable auto-clear
        printMessage(static_cast<uint8_t>(line | 0x80), text, startX);
    }


private:
    // Internal structure mapping the safe asynchronous data pipeline
    struct DisplayMessage {
        uint8_t targetLine;
        uint8_t startX;
        char content[24];
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

/**
 * @brief Run a comprehensive visual test suite and clear the screen after 5 seconds.
 *        Note: This is a global helper function decoupled from the main class interface.
 * @param display Pointer to the active DisplayController instance.
 */
void run_display_font_test(DisplayController* display);
