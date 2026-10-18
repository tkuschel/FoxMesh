/**
 * @file main.cpp
 * @brief Main application entry point for the FoxMesh tracking node
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
//#include "driver/spi_master.h"
#include "esp_log.h"
#include "config.hpp"
#include "led.hpp"
#include "cli.hpp"
#include "i2c.hpp"
#include "display.hpp" // FIXED: Added missing header for DisplayController
#include "lora.hpp"

/**
 * @brief Global log categorization tag context for the main application loader.
 */
static const char *TAG = "MAIN";

/**
 * @brief Global pointer tracking the synchronized thread-safe I2C master bus instance.
 */
i2c* i2c_bus = nullptr;

/**
 * @brief Global pointer tracking the asynchronous isolated OLED render controller instance.
 */
DisplayController* display = nullptr;

/**
 * @brief Global pointer tracking the Semtech SX1276 core radio frequency
*/
LoraController* lora_module = nullptr;

/**
 * @brief Static singleton instance governing hardware status LED blink patterns.
 */
static LedController led(Config::LED_GPIO_PIN);

// Global application anchor marked as extern "C" for the native ESP-IDF bootloader
extern "C" {
    /**
     * @brief System core entry point executing synchronous initialization loops.
     *
     * Spawns the decoupled system worker threads, establishes thread-safe hardware
     * bus routing layers, and boots up the interactive terminal environment on Core 0.
     */
    void app_main(void) {
        // Output system banner configuration parameters on boot
        ESP_LOGI(TAG, "=================================================");
        ESP_LOGI(TAG, " Booting %s (Version: %s) via ESP-IDF C++",
            Config::PROJECT_TITLE,
            Config::PROJECT_VERSION);
        ESP_LOGI(TAG, " (c) Authors: %s", Config::PROJECT_AUTHORS);
        ESP_LOGI(TAG, "=================================================");

        // Launch background blink task
        led.startTask();

        // Dynamically instantiate the synchronized I2C master bus
        i2c_bus = new i2c(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN, Config::OLED_I2C_FREQ_HZ);

        // Instantiate and activate the isolated background render worker on Core 0
        display = new DisplayController(i2c_bus);
        if (display != nullptr) {
            display->startTask();
        }

        // Pass the bus instance dependency to the interactive terminal layer
        cli_init(i2c_bus, &led, display);

        // Spin up the background console interface thread on Core 0
        start_cli_task();

        // LORA initialization and activation
        lora_module = new LoraController();

        if (lora_module != nullptr) {
            //connects the lora instance with the CLI control directly (commands freq and bw)
            cli_init_lora(lora_module);

            // start the radio task straight to the Core 1 (Priority 6)
            lora_module->startTask(6);
            ESP_LOGI(TAG, "LoRa subsystem successfully activated on Core 1.");
        } else {
            ESP_LOGI(TAG, "Critical: Failed to allocate memory for LoraController!");
        }

        // Trigger synchronous welcome splash screen frame
        if (display != nullptr) {
            char msg[32];
            snprintf(msg, sizeof(msg), "FoxMesh %s OE3TKT", Config::PROJECT_VERSION);
            display->printMessage(1, msg);
        }

        // Main system watchdog and status monitor loop
        while (true) {
            ESP_LOGI(TAG, "[%s v%s] Operational. Free Heap: %lu bytes",
                 Config::PROJECT_TITLE,
                 Config::PROJECT_VERSION,
                 (unsigned long)esp_get_free_heap_size());

            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}
