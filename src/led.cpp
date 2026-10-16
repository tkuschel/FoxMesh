#include "led.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.hpp"

// Initialize the static log tag
const char* LedController::TAG = "LED";

LedController::LedController(gpio_num_t gpioPin) : m_gpioPin(gpioPin) {
    // Hardware initialization inside the constructor guarantees safe state upon creation
    gpio_reset_pin(m_gpioPin);
    gpio_set_direction(m_gpioPin, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "Hardware initialized on GPIO %d.", m_gpioPin);
}

void LedController::startTask(uint32_t priority, BaseType_t coreId) {
    // Pass 'this' (the pointer to the current instance) as the task parameter
    xTaskCreatePinnedToCore(
        LedController::taskTrampoline,
        "LEDTask",
        3072,
        this,
        priority,
        nullptr,
        coreId
    );
}

void LedController::taskTrampoline(void* pvParameters) {
    // Cast the raw object pointer back to our class instance
    auto* instance = static_cast<LedController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }

    // Safety fallback: FreeRTOS tasks must never return natively
    vTaskDelete(nullptr);
}

void LedController::run() {
    ESP_LOGI(TAG, "FreeRTOS instance task successfully started on Core %d.", xPortGetCoreID());

    // Define the fixed ON-time as a local constant
    constexpr uint32_t led_on_ms = 30;

    // Compile-time assertion to ensure the interval is always larger than the flash duration
    static_assert(Config::BLINK_INTERVAL_MS > led_on_ms, "Global blink interval must be greater than 30ms!");

    while (true) {
        gpio_set_level(m_gpioPin, 1);    // LED on (High)
        vTaskDelay(pdMS_TO_TICKS(led_on_ms));   // 30ms pulse width

        gpio_set_level(m_gpioPin, 0);    // LED off (Low)
        vTaskDelay(pdMS_TO_TICKS(Config::BLINK_INTERVAL_MS - led_on_ms)); // typ. 1970ms period low time
    }
}
