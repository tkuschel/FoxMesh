/**
 * @file led.hpp
 * @brief Thread-safe hardware LED controller with dynamic profile configuration
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "config.hpp"

class LedController {
public:
    // Compound structure holding the full runtime timing behavior profiles
    struct BlinkProfile {
        uint32_t intervalMs;   // Total period duration of one cycle
        uint32_t pulseWidthMs; // Active high execution duty duration (on-time)
    };

    explicit LedController(gpio_num_t gpioPin = Config::LED_GPIO_PIN);
    virtual ~LedController();

    // Disallow copying to prevent multiple runtime instances controlling the same hardware rail
    LedController(const LedController &) = delete;
    LedController &operator=(const LedController &) = delete;

    /**
     * @brief Spawns the internal FreeRTOS worker thread pinned to a specific core.
     * @param priority Scheduler task priority level.
     * @param coreId Destination CPU core ID anchor (0 or 1).
     */
    void startTask(uint32_t priority = 5, BaseType_t coreId = 1);

    /**
     * @brief Thread-safe atomic updates to configuration parameters from any thread core.
     * @param profile Target values mapping the new timing configuration layers.
     */
    void updateProfile(const BlinkProfile &profile);

private:
    const gpio_num_t m_gpioPin;
    static const char *TAG;

    // Track the internal task handle to allow cross-thread signaling via notifications
    TaskHandle_t m_taskHandle = nullptr;

    // Mutex lock handle safeguarding cross-core profile modifications
    SemaphoreHandle_t m_mutexHandle = nullptr;

    // Internal state profile configuration storage cache
    BlinkProfile m_currentProfile;

    static void taskTrampoline(void *pvParameters);
    void run();
};
