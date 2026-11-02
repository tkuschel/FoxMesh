/**
 * @file main.cpp
 * @brief Main application entry point for the FoxMesh tracking node
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include <cerrno>
#ifndef GIT_TAG
  #define GIT_TAG "unknown"
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h" // Modern ESP-IDF I2C drivers
#include "esp_log.h"
#include "gnss_sync.hpp"
#include "config.hpp"
#include "led.hpp"
#include "cli.hpp"
#include "i2c.hpp"
#include "display.hpp" // FIXED: Added missing header for DisplayController
#include "lora.hpp"
#include "pmu.hpp"

/**
 * @brief Global log categorization tag context for the main application loader.
 */
static const char *TAG = "MAIN";

/**
 * @brief Global pointer tracking the synchronized thread-safe I2C master bus instance.
 */
i2c* i2c_bus = nullptr;

/**
 * @brief Global native ESP-IDF device handle specifically tracking the AXP2101 PMIC.
 */
static i2c_master_dev_handle_t pmic_device_handle = nullptr;

/**
 * @brief Global pointer tracking the asynchronous isolated OLED render controller instance.
 */
DisplayController* display = nullptr;

/**
 * @brief Global pointer tracking the Semtech SX1276 core radio frequency
*/
LoraController* lora_module = nullptr;

/**
 * @brief Global pointer tracking the synchronized high-precision GNSS instance.
 */
GnssTimeSync* gnss_system = nullptr;

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

/**
 * @brief Low-level helper to write a byte to the AXP2101 PMIC via native I2C.
 *
 * Necessary to wake up the power rails before any object lifecycles spin up.
 */
esp_err_t native_axp2101_write(uint8_t reg, uint8_t data) {
    if (pmic_device_handle == nullptr) {
        ESP_LOGE(TAG, "PMIC I2C device handle is uninitialized!");
        return ESP_ERR_INVALID_STATE;
    }

    // Assuming your custom i2c class wrapper has a raw write method, or use native ESP-IDF:
    // This uses a quick transmission fallback to configure the rails immediately
    uint8_t write_buf[2] = {reg, data};

    // Transmit the 2 bytes (Register Address + Data Byte) with no timout constraint (-1)
    esp_err_t err = i2c_master_transmit(pmic_device_handle, write_buf, sizeof(write_buf), -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Failed to Reg 0x%02X: %s", reg, esp_err_to_name(err));
    }
    // Note: Adjust this to use your custom i2c_bus wrapper if preferred!
    // For absolute early boot stability, we write directly to AXP2101 address (0x34)
    #ifdef BOARD_I2C_SDA
        // If your platformio.ini macros are injected, we can safely use them here
        // target address 0x34 is the hardware hardcoded AXP2101 slave identity
    #endif
    return err;
}

/**
 * @brief Power Management initialization sequence for ESP32-S3 targets.
 *
 * Wakes up the essential power rails (ALDO3 for LoRa, ALDO4 for GPS) on the T-Beam Supreme.
 */
void init_power_management(void) {
    ESP_LOGI(TAG, "Initializing AXP2101 PMIC on 400kHz Fast-Mode...");

    // 1. Set ALDO3 (LoRa Transceiver VCC) to 3.3V (0x1A calculation maps to 3.3V)
    native_axp2101_write(0x94, 0x1A);

    // 2. Set ALDO4 (u-blox M10S VCC) to 3.3V
    native_axp2101_write(0x95, 0x1A);

    // 3. Enable ALDO1, ALDO3, and ALDO4 channels simultaneously
    native_axp2101_write(0x90, 0x1D);

    ESP_LOGI(TAG, "PMU power rails activated successfully.");
}

void init_lora_spi(spi_host_device_t host) {
    esp_err_t ret;

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = Config::LORA_MOSI_PIN;
    buscfg.miso_io_num = Config::LORA_MISO_PIN;
    buscfg.sclk_io_num = Config::LORA_SCK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;


    // Initialize the physical peripheral bus with automatic DMA channel assignment
    ret = spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI bus initialized successfully.");
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

        // Dynamically instantiate the synchronized I2C master bus at 400kHz Fast-Mode
        // Config::OLED_I2C_FREQ_HZ should now point to 400000 for optimized performance
        i2c_bus = new i2c(Config::OLED_SDA_PIN, Config::OLED_SCL_PIN, Config::OLED_I2C_FREQ_HZ);

        // CRITICAL T-BEAM SUPREME STEP: Initialize power management right after I2C is alive
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // 1. Initialize a dedicated native I2C master bus for the PMIC on Pins 42/41
        i2c_master_bus_config_t pmic_bus_config = {};
        pmic_bus_config.clk_source       = I2C_CLK_SRC_DEFAULT;
        pmic_bus_config.i2c_port         = I2C_NUM_1; // Use I2C_NUM_1 to not conflict with your custom i2c class on I2C_NUM_0
        pmic_bus_config.scl_io_num       = (gpio_num_t)41; // Fixed Native T-Beam S3 Supreme SCL Pin
        pmic_bus_config.sda_io_num       = (gpio_num_t)42; // Fixed Native T-Beam S3 Supreme SDA Pin
        pmic_bus_config.glitch_ignore_cnt = 7;
        pmic_bus_config.flags.enable_internal_pullup = true;

        i2c_master_bus_handle_t pmic_master_bus_handle;
        esp_err_t bus_err = i2c_new_master_bus(&pmic_bus_config, &pmic_master_bus_handle);

        if (bus_err == ESP_OK) {
            // 2. Register the AXP2101 device onto this dedicated system bus
            PowerManagement *pmu = new PowerManagement(pmic_master_bus_handle, 0x34);
               if (pmu->init() == ESP_OK) {
                   // Pulse the LoRa Reset pin once power is fully stabilized to un-stick the SX1262
                   ESP_LOGI(TAG, "Executing LoRa transceiver hardware pulse sequence...");
                   gpio_set_direction(Config::LORA_RST_PIN, GPIO_MODE_OUTPUT);
                   gpio_set_level(Config::LORA_RST_PIN, 0);
                   vTaskDelay(pdMS_TO_TICKS(50));
                   gpio_set_level(Config::LORA_RST_PIN, 1);
                   vTaskDelay(pdMS_TO_TICKS(50));
               }
        } else {
            ESP_LOGE(TAG, "Failed to initialize native PMIC I2C master bus: %s", esp_err_to_name(bus_err));
        }
#endif

        // Instantiate and activate the isolated background render worker on Core 0
        display = new DisplayController(i2c_bus);
        if (display != nullptr) {
            display->startTask();
        }

        // Pass the bus instance dependency to the interactive terminal layer
        cli_init(i2c_bus, &led, display);



        // Spin up the background console interface thread on Core 0
        start_cli_task();



        // ESP32-S3 (T-Beam Supreme) highly prefers SPI2_HOST (FSPI) for custom IO layouts,
        // while classic ESP32 (TTGO) typically maps to SPI3_HOST (VSPI).
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        spi_host_device_t selected_spi_host = SPI2_HOST;
#else
        spi_host_device_t selected_spi_host = SPI3_HOST;
#endif
        // Initialize the Semtech LoRa SPI connection layer
        init_lora_spi(selected_spi_host);

        // LORA initialization and activation
        lora_module = new LoraController(selected_spi_host);

        if (lora_module != nullptr) {
            //connects the lora instance with the CLI control directly (commands freq and bw)
            cli_init_lora(lora_module);

            // start the radio task strict to the Core 1 (Priority 6)
            lora_module->startTask(6);
            ESP_LOGI(TAG, "LoRa subsystem successfully activated on Core 1.");
        } else {
            ESP_LOGE(TAG, "Critical: Failed to allocate memory for LoraController!");
        }

        // Trigger synchronous welcome splash screen frame
        if (display != nullptr) {
            char msg[32];
            snprintf(msg, sizeof(msg), "FoxMesh " GIT_TAG);
            display->printMessage(0, msg);
            // display->appendMessage(0, GIT_TAG, 12*6);
            display->printMessage(1, "----------------------");
        }

        // 1. Instantiate the single GNSS orchestrator object injecting resources via constructor paths
        gnss_system = new GnssTimeSync(
            (uart_port_t)Config::GPS_UART_NUM, // Injected UART1 hardware controller index
            Config::GPS_RX_PIN,                // GPIO 9 (ESP TX line connected to u-blox RX)
            Config::GPS_TX_PIN,                // GPIO 8 (ESP RX line connected to u-blox TX)
            Config::GPS_PPS_PIN                // GPIO 6 (High-precision hardware PPS input channel)
        );

        // 2. Arm the hardware ISR interrupt vectors, configure UART registers and spin up tasks
        gnss_system->startGnssSync();

        // Main system watchdog and status monitor loop
        while (true) {
            // 1. Log generic system operational and resource data
            ESP_LOGI(TAG, "[%s v%s] Operational. Free Heap: %lu bytes",
                     Config::PROJECT_TITLE,
                     Config::PROJECT_VERSION,
                     (unsigned long)esp_get_free_heap_size());

            // Optional Git footprint reference trace
            // ESP_LOGI(TAG, "Git-Hash: %s", GIT_HASH);

            // 2. Fetch and log thread-safe data snapshot from the GNSS module
            gnss_data_t gps_snapshot;
            if (gnss_system->getLatestData(&gps_snapshot)) {
                if (gps_snapshot.fix_quality > 0) {
                    // High-precision geographic metrics representation with explicit N/S and E/W direction parsing
                    ESP_LOGI(TAG, "GNSS Location: %02d:%02d:%02d UTC | Lat: %.6f %c | Lon: %.6f %c | Alt: %.1fm | Fix: %d",
                             gps_snapshot.hour, gps_snapshot.minute, gps_snapshot.second,
                             std::abs(gps_snapshot.latitude),  (gps_snapshot.latitude >= 0) ? 'N' : 'S',
                             std::abs(gps_snapshot.longitude), (gps_snapshot.longitude >= 0) ? 'E' : 'W',
                             gps_snapshot.altitude,
                             gps_snapshot.fix_quality);
                } else {
                    ESP_LOGI(TAG, "GNSS Engine searching for satellites (No Fix)...");
                }
            } else {
                ESP_LOGE(TAG, "Failed to retrieve a valid thread-safe data snapshot from GNSS module mutex.");
            }

            // 3. Yield task execution back to scheduler for 10 seconds
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
}
