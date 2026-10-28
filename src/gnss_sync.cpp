/**
 * @file gnss_sync.hpp
 * @brief High-precision GNSS time synchronization utilizing UART and hardware PPS signaling.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "gnss_sync.hpp"

// #include <cstring>
#include <sys/time.h>
// #include <time.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_flash.h"
#include "stdint.h"

static const char *TAG = "GNSS";

volatile int64_t g_last_pps_us = 0;

// Hardware Interrupt service routine ISR for the PPS signal (requires C linkage b/c C from C++)
extern "C" void IRAM_ATTR pps_isr_handler(void *arg) {
    // this executes precisely at the start of the UTC second
    GnssTimeSync::m_last_pps_us = esp_timer_get_time();
}

// Wi-Fi event handler, also requires C linkage
extern "C" void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {


}
