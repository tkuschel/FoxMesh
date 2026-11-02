/**
 * @file gnss_sync.hpp
 * @brief High-precision GNSS time synchronization utilizing UART and hardware PPS signaling.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h" // needed for mutex
#include "driver/gpio.h"
#include "driver/uart.h"

// Forward declaration with C linkage for the friend ISR handler
extern "C" void pps_isr_handler(void *arg);

/**
 * @brief Structure to store parsed GNSS data
 */
typedef struct {
    double latitude;
    double longitude;
    double altitude;
    int fix_quality;
    int hour;
    int minute;
    int second;
} gnss_data_t;

class GnssTimeSync {
public:
    // Explicitly allow the C-style ISR to write to our static hardware timestamp register
    friend void ::pps_isr_handler(void *arg);

    /**
     * @brief Constructor to dynamically inject hardware configurations from app_main
     * @param uart_num The hardware UART controller index (e.g., UART_NUM_1)
     * @param tx_pin The ESP TX pin connected to u-blox RX
     * @param rx_pin The ESP RX pin connected to u-blox TX
     * @param pps_pin The hardware PPS interrupt input pin
     */
    GnssTimeSync(uart_port_t uart_num, gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t pps_pin);

    /**
     * @brief Connects to Wi-Fi and syncs the initial date/time via NTP.
     */
    bool syncTimeViaWifi(const std::string &ssid, const std::string &password);

    /**
     * @brief Initializes PPS hardware interrupt, UART driver, and starts the processing task
     */
    void startGnssSync();

    /**
     * @brief Thread-safe getter to fetch the latest parsed GNSS metrics
     * @param out_data Output structure pointer where the data will be safely copied to
     * @return true if data was successfully copied, false if the mutex timeout expired
     */
    bool getLatestData(gnss_data_t *out_data);

    uint32_t getTotalBytes();

private:
    uart_port_t m_uart_num;
    gpio_num_t m_tx_pin;
    gpio_num_t m_rx_pin;
    gpio_num_t m_pps_pin;

    QueueHandle_t m_uart_queue;
    TaskHandle_t m_task_handle;
    SemaphoreHandle_t m_data_mutex; // Mutex protecting the data structure shared across cores

    gnss_data_t m_current_data; // Storage container for the latest parsed values
    uint32_t m_total_bytes_received = 0; // NEW: Global byte counter

    // Static member variable to store the exact hardware timestamp of the last PPS pulse.
    static volatile uint64_t m_last_pps_us;

    // FreeRTOS task handler (must be static in C++)
    static void uartEventTask(void *pvParameters);

    // Internal parsing and synchronization logic
    static void parseGgaAndSync(char *gga_line, uint64_t pps_timestamp, GnssTimeSync *instance);
    static double nmeaToDecimal(double nmea_coord, char direction);
};
