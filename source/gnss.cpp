/**
 * @file gnss.cpp
 * @brief GNSS Driver Implementation with Millisecond System Clock Sync
 * @author OM Tom (OE3TKT)
 */

#include "gnss.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esp_log.h"

static const char* TAG = "GNSS";

gnss::gnss(uart_port_t uart_num, int tx_pin, int rx_pin, uint32_t baud) : _uart_num(uart_num) {
    memset(_line_buffer, 0, sizeof(_line_buffer));

    uart_config_t uart_config = {
        .baud_rate           = (int)baud,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk          = UART_SCLK_DEFAULT,
        .flags               = {0},
    };

    // Install driver with an optimized 256-byte RX buffer (plenty for byte-by-byte polling)
    ESP_ERROR_CHECK(uart_driver_install(_uart_num, 512, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(_uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

gnss::~gnss() {
    uart_driver_delete(_uart_num);
}

bool gnss::update() {
    uint8_t byte;
    bool new_fix_received = false;

    // Drain the hardware UART FIFO buffer
    while (uart_read_bytes(_uart_num, &byte, 1, 0) > 0) {
        if (byte == '\n' || byte == '\r') {
            if (_buffer_idx > 0) {
                _line_buffer[_buffer_idx] = '\0';

                // Validate standard NMEA frame format
                if (_line_buffer[0] == '$') {
                    parse_nmea_sentence(_line_buffer);
                    if (_latest_data.has_fix && strstr(_line_buffer, "GGA") != nullptr) {
                        new_fix_received = true;
                    }
                }
                _buffer_idx = 0;
            }
        } else if (_buffer_idx < sizeof(_line_buffer) - 1) {
            _line_buffer[_buffer_idx++] = byte;
        }
    }
    return new_fix_received;
}

double gnss::convert_nmea_to_decimal(const char* nmea_coord, char direction) {
    if (strlen(nmea_coord) == 0) return 0.0;

    double raw = strtod(nmea_coord, nullptr);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    double decimal = degrees + (minutes / 60.0);

    if (direction == 'S' || direction == 'W') decimal = -decimal;
    return decimal;
}

void gnss::synchronize_system_time(int hours, int minutes, int seconds, int milliseconds) {
    struct tm t = {};
    t.tm_year = _sync_year - 1900;
    t.tm_mon  = _sync_month - 1;   // struct tm expects 0-11
    t.tm_mday = _sync_day;
    t.tm_hour = hours;
    t.tm_min  = minutes;
    t.tm_sec  = seconds;

    time_t time_since_epoch = mktime(&t);
    if (time_since_epoch == -1) return;

    struct timeval tv = {
        .tv_sec = time_since_epoch,
        .tv_usec = milliseconds * 1000
    };

    // Commit precision time leap directly to native ESP32 hardware tracking layer
    settimeofday(&tv, nullptr);
    gettimeofday(&_latest_data.sync_time, nullptr);

    ESP_LOGI(TAG, "System time synchronized via GNSS: %02d:%02d:%02d.%03d",
             hours, minutes, seconds, milliseconds);
}

void gnss::parse_nmea_sentence(const char* sentence) {
    char buf[128];
    // Safeguard memory allocation size constraints
    size_t len = strlen(sentence);
    if (len >= sizeof(buf)) return;
    strcpy(buf, sentence);

    // Extract Date from RMC sentences to ensure timeval translation logic holds true
    if (strstr(sentence, "RMC") != nullptr) {
        char* token = strtok(buf, ",");
        int field_idx = 0;
        char raw_date[16] = {0};

        while (token != nullptr) {
            if (field_idx == 9) { // Field 9 contains date (ddmmyy)
                strncpy(raw_date, token, sizeof(raw_date) - 1);
                break;
            }
            token = strtok(nullptr, ",");
            field_idx++;
        }

        if (strlen(raw_date) == 6) {
            int d = 5, m = 11, y = 26;
            sscanf(raw_date, "%02d%02d%02d", &d, &m, &y);
            _sync_day = d;
            _sync_month = m;
            _sync_year = 2000 + y;
        }
    }

    // Extract positioning telemetry coordinates from high-accuracy GGA frames
    if (strstr(sentence, "GGA") != nullptr) {
        char* token = strtok(buf, ",");
        int field_idx = 0;

        char raw_time[16] = {0};
        char raw_lat[16] = {0}, lat_dir = 'N';
        char raw_lon[16] = {0}, lon_dir = 'E';
        int fix_quality = 0;

        while (token != nullptr) {
            switch (field_idx) {
                case 1: strncpy(raw_time, token, sizeof(raw_time) - 1); break;
                case 2: strncpy(raw_lat, token, sizeof(raw_lat) - 1); break;
                case 3: lat_dir = token[0]; break;
                case 4: strncpy(raw_lon, token, sizeof(raw_lon) - 1); break;
                case 5: lon_dir = token[0]; break;
                case 6: fix_quality = atoi(token); break;
            }
            token = strtok(nullptr, ",");
            field_idx++;
        }

        if (fix_quality > 0 && strlen(raw_time) >= 6) {
            _latest_data.has_fix = true;
            _latest_data.latitude = convert_nmea_to_decimal(raw_lat, lat_dir);
            _latest_data.longitude = convert_nmea_to_decimal(raw_lon, lon_dir);

            int h = 0, m = 0, s = 0, ms = 0;
            sscanf(raw_time, "%02d%02d%02d.%03d", &h, &m, &s, &ms);
            synchronize_system_time(h, m, s, ms);
        } else {
            _latest_data.has_fix = false;
        }
    }
}
