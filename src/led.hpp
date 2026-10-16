#ifndef LED_HPP
#define LED_HPP

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

class LedController {
public:
    // Constructor allows passing the GPIO pin (defaults to 25 as requested)
    explicit LedController(gpio_num_t gpioPin = GPIO_NUM_25);

    // Destructor (virtual in case of inheritance)
    virtual ~LedController() = default;

    // Disallow copying to prevent multiple instances controlling the same hardware pin
    LedController(const LedController&) = delete;
    LedController& operator=(const LedController&) = delete;

    // Spawns the FreeRTOS task pinned to a specific core
    void startTask(uint32_t priority = 5, BaseType_t coreId = 1);

private:
    const gpio_num_t m_gpioPin;
    static const char* TAG;

    // Static trampoline function required by the FreeRTOS scheduler
    static void taskTrampoline(void* pvParameters);

    // Dynamic instance method where the actual blinking loop lives
    void run();
};

#endif // LED_HPP
