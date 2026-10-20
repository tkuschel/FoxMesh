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
#include "driver/spi_master.h"
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

/**
 * @brief Initializes the physical SPI peripheral for the Semtech SX1276 transceiver.
 *
 * Configures the VSPI (SPI3_HOST) back-end utilizing strict compile-time layouts
 * mapped inside config.hpp and configures automatic DMA channel routing.
 */
void init_lora_spi(void) {
    esp_err_t ret;

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = Config::LORA_MOSI_PIN;
    buscfg.miso_io_num = Config::LORA_MISO_PIN;
    buscfg.sclk_io_num = Config::LORA_SCK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;

    // Initialize the physical peripheral bus with automatic DMA channel assignment
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI bus initialized successfully on VSPI (SPI3)");
}

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

        // Initialize the Semtech LoRa SPI connection layer
        init_lora_spi();

        // Spin up the background console interface thread on Core 0
        start_cli_task();

        // LORA initialization and activation
        lora_module = new LoraController();

        if (lora_module != nullptr) {
            //connects the lora instance with the CLI control directly (commands freq and bw)
            cli_init_lora(lora_module);

            // start the radio task strict to the Core 1 (Priority 6)
            lora_module->startTask(6);
            ESP_LOGI(TAG, "LoRa subsystem successfully activated on Core 1.");
        } else {
            ESP_LOGI(TAG, "Critical: Failed to allocate memory for LoraController!");
        }

        // Trigger synchronous welcome splash screen frame
        if (display != nullptr) {
            char msg[32];
            //display->printMessage(0, "FoxMesh " Config::PROJECT_VERSION " \x86" "OE3TKT");
            snprintf(msg, sizeof(msg), "FoxMesh %s", Config::PROJECT_VERSION);
            display->printMessage(0, msg);
            display->appendMessage(0, "OE3TKT", 15*6);
            display->printMessage(1, "----------------------");
            //display->printMessage(2, "Bärbel frühstückt ©");
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
