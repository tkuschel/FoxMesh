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
#include <sys/time.h> // Hinzugefügt für präzise Zeitstrukturen
#include <time.h>     // Hinzugefügt für Zeitkonvertierungen
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "config.hpp"
#include "i2c.hpp"
#include "axp2101.hpp"
#include "gnss.hpp"   // Hinzugefügt für die GNSS-Klasse

static const char *TAG = "main";

// Global application anchor marked as extern "C" for the native ESP-IDF bootloader
extern "C" {
    /**
     * @brief System core entry point executing synchronous initialization loops.
     */
    void app_main(void) {
        // Output system banner configuration parameters on boot
        ESP_LOGI(TAG, "=================================================");
        ESP_LOGI(TAG, " Booting %s (Version: %s) via ESP-IDF C++",
            Config::PROJECT_TITLE,
            Config::PROJECT_VERSION);
        ESP_LOGI(TAG, " (c) Authors: %s", Config::PROJECT_AUTHORS);
        ESP_LOGI(TAG, "=================================================");

        vTaskDelay(pdMS_TO_TICKS(10000));

        // Dynamically instantiate the synchronized I2C master bus at 400kHz Fast-Mode
        i2c internal_bus(I2C_NUM_0, Config::I2C_SDA_PIN, Config::I2C_SCL_PIN, Config::I2C_FREQ_HZ);
        internal_bus.scan();

        // Bus 1 & PMU Initialization via safe stack allocation (ESP32-S3 only)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        i2c pmu_bus(I2C_NUM_1, Config::I2C1_SDA_PIN, Config::I2C1_SCL_PIN, Config::I2C1_FREQ_HZ);
        pmu_bus.scan();

        axp2101 pmic(pmu_bus.get_bus_handle());

        if (!pmic.is_ready()) {
            ESP_LOGE(TAG, "AXP2101 PMIC device registration failed!");
        } else {
            ESP_LOGI(TAG, "AXP2101 Driver successfully attached to I2C Bus 1.");
            // Safely initialize the telemetry hardware channels

           // vTaskDelay(pdMS_TO_TICKS(100));

            pmic.begin();

            // power up GNSS module uBlox
            pmic.set_gps_power(true);
        }
#endif

        // GNSS-Empfänger instanziieren (Nutzt UART_NUM_1, TX=8, RX=9, 9600 Baud)
        gnss gps_receiver(UART_NUM_1, 8, 9, 9600);
        ESP_LOGI(TAG, "GNSS Driver Subsystem initialized.");

        // Zeitstempel für das periodische 10-Sekunden-Watchdog-Intervall
        TickType_t last_watchdog_tick = xTaskGetTickCount();

        // Main system watchdog and status monitor loop
        while (true) {
            // GNSS-Treiber im Loop füttern. Liest Zeichen um Zeichen aus dem UART-Buffer.
            if (gps_receiver.update()) {
                gnss::gps_data_t current_gps = gps_receiver.get_latest_data();

                if (current_gps.has_fix) {
                    struct timeval tv;
                    gettimeofday(&tv, nullptr); // Aktuelle ESP-Systemzeit holen

                    struct tm timeinfo;
                    gmtime_r(&tv.tv_sec, &timeinfo); // In UTC konvertieren

                    ESP_LOGI("TRACKER", "--- GNSS TELEMETRY PACKET ---");
                    ESP_LOGI("TRACKER", "Position: Lat: %.6f | Lon: %.6f", current_gps.latitude, current_gps.longitude);
                    ESP_LOGI("TRACKER", "Internal Precision Clock: %02d:%02d:%02d.%03ld UTC",
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, tv.tv_usec / 1000);
                    ESP_LOGI("TRACKER", "-----------------------------");
                }
            }

            // Da vTaskDelay am Ende des Loops durch das non-blocking Daten-Polling ersetzt wurde,
            // prüfen wir hier mathematisch exakt, ob 10 Sekunden (10000 ms) vergangen sind.
            if ((xTaskGetTickCount() - last_watchdog_tick) >= pdMS_TO_TICKS(10000)) {
                // Log generic system operational and resource data
                ESP_LOGI(TAG, "[%s v%s] Operational. Free Heap: %lu bytes",
                         Config::PROJECT_TITLE,
                         Config::PROJECT_VERSION,
                         (unsigned long)esp_get_free_heap_size());

#if defined(CONFIG_IDF_TARGET_ESP32S3)
                if (pmic.is_ready()) {
                    bool bat_present = false;
                    uint16_t bat_voltage = 0;

                    if (pmic.is_battery_connected(bat_present) == ESP_OK) {
                        ESP_LOGI(TAG, "Battery Connected: %s", bat_present ? "YES" : "NO");
                        if (bat_present && pmic.get_battery_voltage(bat_voltage) == ESP_OK) {
                            ESP_LOGI(TAG, "Battery Voltage: %u mV (%.2f V)", bat_voltage, bat_voltage / 1000.0f);
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to communicate with AXP2101 PMIC.");
                    }
                }
#endif
                last_watchdog_tick = xTaskGetTickCount(); // Timer zurücksetzen
            }

            // Sehr kurzes Yield (1 Tick, ca. 10ms), damit der Watchdog nicht triggert
            // und die CPU atmen kann, wir aber kein einziges UART-Zeichen verlieren.
            vTaskDelay(1);
        }
    }
}
