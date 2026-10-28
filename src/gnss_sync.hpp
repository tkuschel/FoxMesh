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

/**
 * @brief Structure to store parsed GNSS data
*/
typedef struct {
    float latitude;
    float longitude;
    float altitude;
    int fix_quality;
    int hour;
    int minute;
    int second;
} gnss_data_t;

extern "C" void pps_isr_handler(void *arg);


class GnssTimeSync {

public:

    friend void ::pps_isr_handler(void *arg);


    /**
     * @brief Connects to Wi-Fi and syncs the intial date/time via NTP
     *        Blocks for up to 20 seconds.
    */
    bool syncTimeViaWifi(const std::string &ssid, const std::string &password);

    /**
     * @brief Initializes PPS hardware interrupt, UART driver, and starts the processing task
     */
    void startGnssSync();

private:
    int m_uart_num;
    int m_tx_pin;
    int m_rx_pin;
    int m_pps_pin;

    QueueHandle_t m_uart_queue;
    TaskHandle_t m_task_handle;


    // NO GLOBAL VARIABLES:
    // Static member variable to store the exact hardware timestamp of the last PPS pulse.
    // 'inline' allows initialization directly inside the header (C++17 and newer).
    // 'volatile' ensures the compiler doesn't optimize away reads/writes from the ISR.
    static inline volatile uint64_t m_last_pps_us = 0;

    // FreeRTOS task handler (must be static in C++)
    static void uartEventTask(void *pvParameters);

    // Internal parsing and synchronization logic
    static void parseGgaAndSync(char *gga_line, uint64_t pps_timestamp);
    static float nmeaToDecimal(float nmea_coord, char direction);
};
