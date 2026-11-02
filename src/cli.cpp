/**
 * @file cli.cpp
 * @brief Interactive asynchronous Command Line Interface implementation for FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "cli.hpp"
#include "commands.hpp"
#include "gnss_sync.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "config.hpp"
#include "lora.hpp"
#include "display.hpp"
#include "esp_vfs_dev.h"

// Unified low-level hardware drivers for both targets
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "driver/usb_serial_jtag.h"
// #include "esp_vfs_usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#endif

static const char *TAG = "CLI";
static i2c *cli_i2c_bus = nullptr;

/**
 * Hardware singletons used across the system.
 * REMOVED 'static' so commands.cpp can access them via extern declarations!
 */
LedController *led_system = nullptr;
DisplayController *oled_display = nullptr;
LoraController *lora_system = nullptr;
GnssTimeSync *gnss_system = nullptr; // new global pointer to GNSS

#define CLI_UART_PORT UART_NUM_0
#define BUF_SIZE 1024

/**
 * @brief Handle a silence timer for suppressing logs during interactive CLI actions.
 */
TimerHandle_t silence_timer = nullptr;

/**
 * @brief Dynamic configuration profile holding the current logging suppression runtime in seconds.
 */
uint32_t current_mute_time_sec = Config::DEFAULT_MUTE_TIME_SEC;

/**
 * @brief Initializes the internal pointers to project singletons.
 * @param bus_pointer Active I2C master bus context.
 * @param led_pointer Active status LED task instance.
 * @param display_pointer Active OLED worker instance.
 */
void cli_init(i2c *bus_pointer, LedController *led_pointer, DisplayController *display_pointer) {
    cli_i2c_bus = bus_pointer;
    led_system = led_pointer;
    oled_display = display_pointer;
}

void cli_init_lora(LoraController *lora_pointer) {
    lora_system = lora_pointer;
}

/**
 * @brief Software timer callback task executed once terminal interaction has gone stale.
 * @param xTimer Handle tracking the specific FreeRTOS timing token instance.
 */
static void log_restore_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer; // Prevent compiler warning about unused parameter

    CommandManager *manager = get_command_manager_instance();

    // Print a clean new prompt since logs might have disrupted the visual terminal interface
    printf("\r\n[System] Debug logs unmuted after %lu seconds of inactivity. \r\n", (unsigned long)current_mute_time_sec);

    if (manager->getOperationMode() == OperationMode::ONLINE_MODE) {
        const char *target = manager->getActiveTarget().c_str();
        printf("@%s> ", target ? target : "all");
    } else {
        printf("%s # ", Config::PROJECT_TITLE);
    }
    fflush(stdout);

    // Securely restore native logging profiles back to application default INFO metrics
    esp_log_level_set("*", ESP_LOG_INFO);
}

/**
 * @brief Infinite FreeRTOS task handling clean, low-overhead collection of serial input characters.
 * @param pvParameters Context input pointer passed by the task scheduler (unused).
 */
static void cli_worker_task(void *pvParameters) {
    (void)pvParameters;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    /* --- ESP32-S3 Native USB-CDC Driver Setup --- */
    usb_serial_jtag_driver_config_t usb_config = {};
    usb_config.rx_buffer_size = BUF_SIZE;
    usb_config.tx_buffer_size = BUF_SIZE;

    // Install the JTAG/Serial hardware driver cleanly
    esp_err_t usb_err = usb_serial_jtag_driver_install(&usb_config);
    if (usb_err != ESP_OK && usb_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to mount USB Serial core hardware driver.");
    }

    // Bind USB-SERIAL-JTAG directly into virtual file system console output endpoints
    //esp_vfs_usb_serial_jtag_use_driver();
    // Bind USB-SERIAL-JTAG directly into virtual file system console output endpoints
    usb_serial_jtag_vfs_use_driver();

    // Disable stdout block line buffering natively across the runtime layers
    setvbuf(stdout, NULL, _IONBF, 0);
#else
    /* --- Classic TTGO UART0 Hardware Setup --- */
    uart_config_t uart_config = {};
    uart_config.baud_rate     = 115200;
    uart_config.data_bits     = UART_DATA_8_BITS;
    uart_config.parity        = UART_PARITY_DISABLE;
    uart_config.stop_bits     = UART_STOP_BITS_1;
    uart_config.flow_ctrl     = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk    = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(CLI_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(CLI_UART_PORT, BUF_SIZE * 2, 0, 0, nullptr, 0));
#endif
    // Mount object-oriented core command system manager structures
    initialize_commands_system();
    CommandManager *manager = get_command_manager_instance();

    char line_buffer[BUF_SIZE];
    int buffer_index = 0;
    bool last_was_newline = false;
    bool cli_activated = false; // At the very beginning: CLI sleeps


    while (1) {
        // AUTOMATIC DISCONNECT DETECTION (ESP32-S3 Native USB only!)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        if (!usb_serial_jtag_is_connected()) {
            cli_activated = false;
        }
#endif

        uint8_t data = 0;
        int len = 0;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // Highly efficient blocking hardware read via USB controller interrupts (20ms yield timeout)
        len = usb_serial_jtag_read_bytes(&data, 1, pdMS_TO_TICKS(20));
#else
        // Highly efficient blocking hardware read via UART controller interrupts (20ms yield timeout)
        len = uart_read_bytes(CLI_UART_PORT, &data, 1, pdMS_TO_TICKS(20));
#endif

        if (len > 0) {
            // Suppress system debug verbosity during active user typing interaction
            esp_log_level_set("*", ESP_LOG_NONE);

            if (silence_timer != nullptr && current_mute_time_sec != 0) {
                xTimerReset(silence_timer, pdMS_TO_TICKS(10));
            }

            // Capture Carriage Return or Line Feed sequence boundaries
            if (data == '\r' || data == '\n') {
                if (!cli_activated) {
                    cli_activated = true;
                    printf("\r\n=== Interactive %s CLI Console Started ===\r\n", Config::PROJECT_TITLE);

                    // Check the current operational mode to print a helpful hint
                    if (manager->getOperationMode() == OperationMode::ONLINE_MODE) {
                        // Print comprehensive identity and network routing context
                        printf("Mode     : TRANSPARENT ONLINE CHAT\r\n");
                        printf("Identity : %s-%s\r\n", manager->getCallsign().c_str(), manager->getSsid().c_str());
                        printf("Target   : @%s\r\n", manager->getActiveTarget().c_str());
                        printf("Hint     : Type your message to broadcast, or type '+++' for COMMAND mode.\r\n\r\n");
                        printf("@%s> ", manager->getActiveTarget().c_str());
                    } else {
                        printf("Mode     : COMMAND EXECUTION LAYER\r\n");
                        printf("Hint     : Type 'help' or '?' to get a list of active instructions.\r\n\r\n");
                        printf("%s # ", Config::PROJECT_TITLE);                    }
                    fflush(stdout);
                    last_was_newline = true;
                    buffer_index = 0;
                } else if (buffer_index > 0) {
                    line_buffer[buffer_index] = '\0';
                    manager->processLine(line_buffer, ResponseChannel::UART);
                    buffer_index = 0;
                    last_was_newline = true;
                } else if (!last_was_newline) {
                    // this handles a single, deliberate press of the Enter key on an empty line
                    line_buffer[0] = '\0';
                    manager->processLine(line_buffer, ResponseChannel::UART);
                    last_was_newline = true;
                }
            }
            // Evaluate Backspace command frames (ASCII 8 or Delete 127 symbols)
            else if ((data == 8 || data == 127) && buffer_index > 0) {
                buffer_index--;
                printf("\b \b");
                fflush(stdout);
                // Reset newline tracker on active input
                last_was_newline = false;
            }
            // Sanitize and append terminal symbols (supports multi-byte chars like German Umlaute)
            else if (((data >= 32 && data <= 126) || data >= 128) && buffer_index < (BUF_SIZE - 1)) {
                line_buffer[buffer_index++] = static_cast<char>(data);
                printf("%c", data);
                fflush(stdout);
                // Reset newline tracker on active input
                last_was_newline = false;
            }
        }
    }
}

void start_cli_task(void) {
    // FIXED: Pre-calculated dynamic period duration variable ensuring safe macro consumption
    uint32_t silence_period_ms = current_mute_time_sec * 1000;

    silence_timer = xTimerCreate(
        "MuteTimer",
        pdMS_TO_TICKS(silence_period_ms),
        pdFALSE,
        nullptr,
        log_restore_timer_callback
    );
    if (silence_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS silence timer.");
    }
    // Allocate resources on Core 0 to decouple console loads from LoRa packet handling on Core 1
    xTaskCreatePinnedToCore(
        cli_worker_task,
        "CLI_InterfaceTask",
        configMINIMAL_STACK_SIZE + 4096,
        nullptr,
        5,
        nullptr,
        0
    );
}
