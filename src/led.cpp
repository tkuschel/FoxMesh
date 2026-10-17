/**
 * @file led.cpp
 * @brief Thread-safe hardware LED controller task implementation with profile selection
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "led.hpp"
#include "esp_log.h"
#include "config.hpp"
#include <climits> // Enforces definitions of macro bounds like ULONG_MAX

const char* LedController::TAG = "LED";

LedController::LedController(gpio_num_t gpioPin) : m_gpioPin(gpioPin) {
    // Core default profile initialized using standard configurations
    m_currentProfile.intervalMs = static_cast<uint32_t>(Config::BLINK_INTERVAL_MS);
    m_currentProfile.pulseWidthMs = 30;

    // Instantiate a lightweight mutex token to guarantee strict isolation
    m_mutexHandle = xSemaphoreCreateMutex();
    if (m_mutexHandle == nullptr) {
        ESP_LOGE(TAG, "Fatal: Failed to allocate mutex tracking resources.");
    }

    gpio_reset_pin(m_gpioPin);
    gpio_set_direction(m_gpioPin, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "Hardware initialized on GPIO %d.", m_gpioPin);
}

LedController::~LedController() {
    if (m_mutexHandle != nullptr) {
        vSemaphoreDelete(m_mutexHandle);
    }
}

void LedController::startTask(uint32_t priority, BaseType_t coreId) {
    xTaskCreatePinnedToCore(
        LedController::taskTrampoline,
        "LEDTask",
        3072,
        this,
        priority,
        &m_taskHandle,
        coreId
    );
}

void LedController::updateProfile(const BlinkProfile& profile) {
    if (profile.pulseWidthMs >= profile.intervalMs) {
        ESP_LOGE(TAG, "Profile rejection: pulseWidthMs must be smaller than intervalMs!");
        return;
    }

    // Safely block writing operations across separate core domains until token release
    if (m_mutexHandle != nullptr && xSemaphoreTake(m_mutexHandle, portMAX_DELAY) == pdTRUE) {
        m_currentProfile = profile;
        xSemaphoreGive(m_mutexHandle);
    }

    // Interrupt the active blocking state of the worker thread instantly
    if (m_taskHandle != nullptr) {
        xTaskNotifyGive(m_taskHandle);
    }
}

void LedController::taskTrampoline(void* pvParameters) {
    auto* instance = static_cast<LedController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }
    vTaskDelete(nullptr);
}

void LedController::run() {
    ESP_LOGI(TAG, "FreeRTOS instance task successfully started on Core %d.", xPortGetCoreID());

    while (true) {
        BlinkProfile active = {};

        // Extract a thread-safe snapshot configuration boundary under lock condition
        if (m_mutexHandle != nullptr && xSemaphoreTake(m_mutexHandle, portMAX_DELAY) == pdTRUE) {
            active = m_currentProfile;
            xSemaphoreGive(m_mutexHandle);
        } else {
            // Safe hardware fallback calculation loops
            active.intervalMs = 1000;
            active.pulseWidthMs = 30;
        }

        if (active.pulseWidthMs == 0) active.pulseWidthMs = 1;
        if (active.pulseWidthMs >= active.intervalMs) active.intervalMs = active.pulseWidthMs + 10;

        // 1. High pulse execution phase
        gpio_set_level(m_gpioPin, 1);
        vTaskDelay(pdMS_TO_TICKS(active.pulseWidthMs));
        gpio_set_level(m_gpioPin, 0);

        // 2. Dynamic evaluation of low-state processing window
        uint32_t remainingLowTimeMs = active.intervalMs - active.pulseWidthMs;

        // Non-polling block sleep that awakes immediately upon configuration adjustments
        xTaskNotifyWait(0x00, ULONG_MAX, nullptr, pdMS_TO_TICKS(remainingLowTimeMs));
    }
}
