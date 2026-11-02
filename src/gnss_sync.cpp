/**
 * @file gnss_sync.cpp
 * @brief High-precision GNSS time synchronization utilizing UART and hardware PPS signaling.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "config.hpp"
#include "gnss_sync.hpp"
#include <sys/time.h>
#include <cstring>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "GNSS";

// Allocate memory for the static member variable tracker
volatile uint64_t GnssTimeSync::m_last_pps_us = 0;

// Hardware Interrupt service routine ISR for the PPS signal
extern "C" void IRAM_ATTR pps_isr_handler(void *arg) {
    (void)arg;
    // Capture the absolute hardware microsecond timestamp at the exact rise of the UTC second
    GnssTimeSync::m_last_pps_us = esp_timer_get_time();
}

// Constructor mapping injected hardware profiles directly onto instance structures
GnssTimeSync::GnssTimeSync(uart_port_t uart_num, gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t pps_pin)
    : m_uart_num(uart_num), m_tx_pin(tx_pin), m_rx_pin(rx_pin), m_pps_pin(pps_pin),
      m_uart_queue(nullptr), m_task_handle(nullptr) {

    m_total_bytes_received = 0;
    // Create a Mutex binary semaphore to safely synchronize multi-core data reads/writes
    m_data_mutex = xSemaphoreCreateMutex();
    std::memset(&m_current_data, 0, sizeof(gnss_data_t));

}

void GnssTimeSync::startGnssSync() {
    ESP_LOGI(TAG, "Initializing GNSS Subsystem (UART %d, PPS Pin: %d)...", m_uart_num, m_pps_pin);

// NEW: Force the u-blox M10S internal hardware enable pin to HIGH to wake up the core
    ESP_LOGI(TAG, "Pulling GPS Hardware Enable Pin (GPIO %d) to HIGH...", Config::GPS_EN_PIN);
    gpio_reset_pin(Config::GPS_EN_PIN);
    gpio_set_direction(Config::GPS_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(Config::GPS_EN_PIN, 1); // Set to 1 (HIGH) to inject enable voltage

    // Give the u-blox M10S chip a short moment to clear its internal bootloader state
    vTaskDelay(pdMS_TO_TICKS(100));

    // 1. Configure the Injected PPS Hardware GPIO Line & Interrupt Hook
    gpio_config_t pps_conf = {};
    pps_conf.pin_bit_mask = (1ULL << m_pps_pin);
    pps_conf.mode         = GPIO_MODE_INPUT;
    pps_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    pps_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pps_conf.intr_type    = GPIO_INTR_POSEDGE; // Trigger precisely on the rising pulse edge
    ESP_ERROR_CHECK(gpio_config(&pps_conf));

    // Install global ISR service if not already present, and hook our specific handler
    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(m_pps_pin, pps_isr_handler, nullptr));

    // 2. Configure Injected UART Driver Controller Layer
    uart_config_t uart_config = {};
    uart_config.baud_rate           = 38400; // Native u-blox M10S default supreme execution speed
    // uart_config.baud_rate = 9600;
    uart_config.data_bits           = UART_DATA_8_BITS;
    uart_config.parity              = UART_PARITY_DISABLE;
    uart_config.stop_bits           = UART_STOP_BITS_1;
    uart_config.flow_ctrl           = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk          = UART_SCLK_DEFAULT;

    const int uart_buffer_size = 2048;
    // Install driver with an assigned event queue pointer
    ESP_ERROR_CHECK(uart_driver_install(m_uart_num, uart_buffer_size, 0, 10, &m_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(m_uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(m_uart_num, m_tx_pin, m_rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 3. Spool up the Asynchronous FreeRTOS Worker Task on Core 0
    xTaskCreatePinnedToCore(
        this->uartEventTask,
        "GNSS_ProcessingTask",
        4096,
        this, // Pass 'this' reference so the static task can access our class instances
        6,
        &m_task_handle,
        0
    );
}

/**
 * @brief Infinite FreeRTOS task handling incoming asynchronous UART data packets and event loops.
 * @param pvParameters Object reference context holding the active 'this' instance pointer.
 */
void GnssTimeSync::uartEventTask(void *pvParameters) {
    // Cast the generic context parameter back to our object structure instance pointer
    GnssTimeSync *instance = static_cast<GnssTimeSync*>(pvParameters);
    uart_event_t event;

    uint8_t *data_buffer = (uint8_t *)malloc(2048);
    if (data_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for GNSS UART buffer stack.");
        vTaskDelete(nullptr);
        return;
    }

    char line_buffer[256];
    int line_index = 0;

    while (1) {
        // High-precision queue block waiting for hardware UART interrupt event notifications
        if (xQueueReceive(instance->m_uart_queue, (void *)&event, portMAX_DELAY)) {
            std::memset(data_buffer, 0, 2048);

            switch (event.type) {
                case UART_DATA: {
                    int bytes_read = uart_read_bytes(instance->m_uart_num, data_buffer, event.size, portMAX_DELAY);
                    if (bytes_read > 0) {
                        instance->m_total_bytes_received += bytes_read;
                    }
                    for (int i = 0; i < bytes_read; i++) {
                        char character = static_cast<char>(data_buffer[i]);

                        // NEW RAW NMEA echo to USB Console for Debugging
                        printf("%c", character);


                        // Accumulate incoming serial stream data bytes into bounded sentence lines
                        if (character == '\r' || character == '\n') {
                            if (line_index > 0) {
                                line_buffer[line_index] = '\0';

                                // Safely verify the header token pattern matches the target GGA message payload
                                if (std::strncmp(line_buffer, "$GNGGA", 6) == 0 || std::strncmp(line_buffer, "$GPGGA", 6) == 0) {
                                    // Capture the snapshot value of the volatile atom edge timestamp before computing strings
                                    uint64_t current_pps_snapshot = instance->m_last_pps_us;
                                    parseGgaAndSync(line_buffer, current_pps_snapshot, instance);
                                }
                                line_index = 0;
                            }
                        } else if (line_index < (sizeof(line_buffer) - 1)) {
                            line_buffer[line_index++] = character;
                        }
                    }
                    break;
                }
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART hardware FIFO overflow detected.");
                    uart_flush_input(instance->m_uart_num);
                    xQueueReset(instance->m_uart_queue);
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART software ring-buffer saturation achieved.");
                    uart_flush_input(instance->m_uart_num);
                    xQueueReset(instance->m_uart_queue);
                    break;
                default:
                    break;
            }
        }
    }
    free(data_buffer);
    vTaskDelete(nullptr);
}

/**
 * @brief Decodes the UTC timestamp from standard GGA frames and calibrates the system clock.
 * @details Extracts high-precision time, position (64-bit double), and fix quality metrics.
 *          It cross-references the delayed serial UART frame with the exact hardware PPS
 *          timestamp snapshot to achieve microsecond-accurate clock alignment via settimeofday().
 * @param gga_line Null-terminated NMEA string containing positional and tracking parameters.
 * @param pps_timestamp The absolute hardware microsecond snapshot tracking the matching UTC second rise.
 * @param instance Pointer to the active object instance for thread-safe cross-core data synchronization.
 */
void GnssTimeSync::parseGgaAndSync(char *gga_line, uint64_t pps_timestamp, GnssTimeSync *instance) {
    int utc_hour = 0, utc_minute = 0, utc_second = 0;
    double raw_lat = 0.0, raw_lon = 0.0, raw_alt = 0.0; // 64-bit double precision tracking variables
    char lat_dir = 'N', lon_dir = 'E';
    int fix_qual = 0;

    // Direct token scanner parsing parameters out of the incoming frame layout strings
    // Fixed: Using %lf for double scanning precision
    int scanned_tokens = sscanf(gga_line, "$%*2cGGA,%2d%2d%2d.%*2d,%lf,%c,%lf,%c,%d,%*d,%*f,%lf",
                                &utc_hour, &utc_minute, &utc_second,
                                &raw_lat, &lat_dir, &raw_lon, &lon_dir, &fix_qual, &raw_alt);

    if (scanned_tokens >= 8) {
        // Secure data copy into instance memory using the Mutex
        if (instance->m_data_mutex != nullptr && xSemaphoreTake(instance->m_data_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            instance->m_current_data.latitude    = nmeaToDecimal(raw_lat, lat_dir);
            instance->m_current_data.longitude   = nmeaToDecimal(raw_lon, lon_dir);
            instance->m_current_data.altitude    = raw_alt;
            instance->m_current_data.fix_quality = fix_qual;
            instance->m_current_data.hour        = utc_hour;
            instance->m_current_data.minute      = utc_minute;
            instance->m_current_data.second      = utc_second;
            xSemaphoreGive(instance->m_data_mutex);
        }
        if (fix_qual > 0 && pps_timestamp > 0) {
            uint64_t host_now_us = esp_timer_get_time();

            // Calculate precisely how many microseconds have elapsed since the high-precision PPS pulse edge triggered
            uint64_t propagation_latency_us = host_now_us - pps_timestamp;

            // Only synchronize clocks if the matching reference anchor pulse edge is fresh (< 1 second)
            if (propagation_latency_us < 1000000) {
                struct tm time_structure = {};

                // Keep in mind: Year, month, day should be initialized previously via RMC or Wifi NTP,
                // as GGA strings exclusively contain sub-day time vectors!
                time_structure.tm_hour = utc_hour;
                time_structure.tm_min  = utc_minute;
                time_structure.tm_sec  = utc_second;
                time_structure.tm_year = 126; // Static mock baseline for year 2026 if uninitialized
                time_structure.tm_mday = 2;   // Static mock baseline for day 2 if uninitialized
                time_structure.tm_mon  = 10;  // Static mock baseline for November if uninitialized

                time_t derived_utc_epoch_seconds = mktime(&time_structure);

                struct timeval high_res_time_value = {};
                high_res_time_value.tv_sec  = derived_utc_epoch_seconds;
                high_res_time_value.tv_usec = static_cast<suseconds_t>(propagation_latency_us); // Inject high-precision sub-second tracking error offset!

                // Commit the atomic clock alignment to the core ESP32-S3 execution kernel
                settimeofday(&high_res_time_value, nullptr);

                ESP_LOGI(TAG, "System UTC time high-precision synced via PPS! Target: %02d:%02d:%02d (Latency: %llu us)",
                         utc_hour, utc_minute, utc_second, (unsigned long long)propagation_latency_us);
            }
        } else {
            ESP_LOGW(TAG, "GNSS Frame parsed but ignored. Tracking lock or valid PPS signature missing.");
        }
    }
}

/**
 * @brief Converts NMEA geographic degree-minutes coordinate layout structures into native Decimal Degrees.
 */
double GnssTimeSync::nmeaToDecimal(double nmea_coord, char direction) {
    // 1. Safely cut off the minutes part to extract pure degrees.
    // Example: 4812.123456 -> 48
    int degrees = static_cast<int>(nmea_coord / 100.0);

    // 2. Isolate the minutes using exact subtraction.
    // 4812.123456 - (48 * 100) = 12.123456 minutes
    double minutes = nmea_coord - (static_cast<double>(degrees) * 100.0);

    // 3. Convert minutes to decimal degrees with 64-bit double precision
    double decimal_val = static_cast<double>(degrees) + (minutes / 60.0);

    // 4. Apply southern or western hemisphere inversion
    if (direction == 'S' || direction == 'W') {
        decimal_val = -decimal_val;
    }
    return decimal_val;
}

bool GnssTimeSync::getLatestData(gnss_data_t *out_data) {
    if (m_data_mutex == nullptr || out_data == nullptr) return false;

    // Take the mutex with a 50ms max block timeout to avoid stalling execution
    if (xSemaphoreTake(m_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        std::memcpy(out_data, &m_current_data, sizeof(gnss_data_t));
        xSemaphoreGive(m_data_mutex); // Release immediately after memory copy
        return true;
    }
    return false; // Mutex was blocked by processing task
}

uint32_t GnssTimeSync::getTotalBytes() {
    return m_total_bytes_received;
}
