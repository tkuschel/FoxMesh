/**
 * @file gnss.hpp
 * @brief GPS/GNSS Telemetry and Time Synchronization Driver for FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/time.h>
#include "driver/uart.h"
#include "driver/gpio.h"

class gnss {
public:
    struct gps_data_t {
        double latitude = 0.0;
        double longitude = 0.0;
        bool has_fix = false;
        struct timeval sync_time = {0, 0}; ///< System time mapped to exact GPS sync moment
    };

    /**
     * @brief Constructor for the GNSS driver subsystem.
     */
    gnss(uart_port_t uart_num, int tx_pin, int rx_pin, uint32_t baud = 9600);
    ~gnss();

    // Disable copy semantics
    gnss(const gnss&) = delete;
    gnss& operator=(const gnss&) = delete;

    /**
     * @brief Parses incoming UART bytes and checks for valid NMEA coordinates/time.
     * @return true if a new valid position/time fix was processed.
     */
    bool update();

    /**
     * @brief Gets the latest validated coordinate and time packet.
     */
    gps_data_t get_latest_data() const { return _latest_data; }

private:
    uart_port_t _uart_num;
    gps_data_t _latest_data;
    char _line_buffer[128]; ///< Secure buffer for a single NMEA sentence (max standard is 82)
    size_t _buffer_idx = 0;

    // Fixed internal date buffers to hold RMC sequence data for high-precision timeval mapping
    int _sync_day = 5;
    int _sync_month = 11;
    int _sync_year = 2026;

    void parse_nmea_sentence(const char* sentence);
    void synchronize_system_time(int hours, int minutes, int seconds, int milliseconds);
    double convert_nmea_to_decimal(const char* nmea_coord, char direction);
};
