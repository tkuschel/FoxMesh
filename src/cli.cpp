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

static const char* TAG = "CLI";
static i2c* i2c_bus = nullptr;
static LedController* led_system = nullptr;
static DisplayController* oled_display = nullptr;
static LoraController* lora_system = nullptr;

#define CLI_UART_PORT UART_NUM_0
#define BUF_SIZE 1024

/**
 * @brief Handle a silence timer for suppressing logs during interactive CLI actions.
 */
static TimerHandle_t silence_timer = nullptr;

/**
 * @brief Dynamic configuration profile holding the current logging suppression runtime in seconds.
 */
static uint32_t current_mute_time_sec = Config::DEFAULT_MUTE_TIME_SEC;

/**
 * @brief Initializes the internal pointers to project singletons.
 * @param bus_pointer Active I2C master bus context.
 * @param led_pointer Active status LED task instance.
 * @param display_pointer Active OLED worker instance.
 */
void cli_init(i2c* bus_pointer, LedController* led_pointer, DisplayController* display_pointer) {
    i2c_bus = bus_pointer;
    led_system = led_pointer;
    oled_display = display_pointer;
}


void cli_init_lora(LoraController* lora_pointer) {
    lora_system = lora_pointer;
}
/**
 * @brief Software timer callback task executed once terminal interaction has gone stale.
 * @param xTimer Handle tracking the specific FreeRTOS timing token instance.
 */
static void log_restore_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer; // Prevent compiler warning about unused parameter

    // Print a clean new prompt since logs might have disrupted the visual terminal interface
    printf("\r\n[System] Debug logs unmuted after %lu seconds of inactivity. \r\n%s #\r\n",
        (unsigned long)current_mute_time_sec, Config::PROJECT_TITLE);
    fflush(stdout);

    // Securely restore native logging profiles back to application default INFO metrics
    esp_log_level_set("*", ESP_LOG_INFO);
}

/**
 * @brief Tokenizes and evaluates commands received over the serial UART layer.
 * @param cmd Raw null-terminated character array.
 */
void process_command(const char* cmd) {
    if (strlen(cmd) == 0) return;

    printf("\r\n");

    if (strcmp(cmd, "help") == 0) {
        printf("Available commands:\r\n");
        printf("  help               - Show this command overview\r\n");
        printf("  version            - Display project title and version\r\n");
        printf("  status             - Print internal hardware statistics\r\n");
        printf("  mute <sec>         - Set custom logging timeout (Current: %lus)\r\n", (unsigned long)current_mute_time_sec);
        printf("  blink <int> <puls> - Set LED profile in ms (e.g. blink 1000 50)\r\n");
        printf("  oled <line> <txt>  - Print text to OLED line 0-7 (e.g. oled 0 Test)\r\n");
        printf("  freq <hz>          - Set dynamic radio frequency (433 / 868 MHz bounds)\r\n");
        printf("  bw <62|125|250|500>- Set dynamic LoRa bandwidth in kHz\r\n");
        printf("  i2c                - Scan the configured I2C bus infrastructure\r\n");
        printf("  test-display       - Run a comprehensive visual test suite\r\n");
    }
    else if (strcmp(cmd, "version") == 0) {
        printf("%s System - Running Version: %s\r\n", Config::PROJECT_TITLE, Config::PROJECT_VERSION);
    }
    else if (strcmp(cmd, "status") == 0) {
        printf("--- %s Status ---\r\n", Config::PROJECT_TITLE);
        printf("Free Internal Heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
        printf("Mute Timer Period : %lu seconds\r\n", (unsigned long)current_mute_time_sec);
        printf("Target CPU Board  : ESP32\r\n");
        printf("ESP-IDF Version   : %s\r\n", esp_get_idf_version());

        if (lora_system != nullptr) {
            // atomar, thread-safe copy of telemtry data (stack allocation)
            LoraStateSnapshot snapshot = lora_system->getSnapshot();

            printf("LoRa Hardware     : %s\r\n", snapshot.isInitialized ? "INITIALIZED" : "FAILED / UNINITIALIZED");
            printf("LoRa Transceiver  : Mode [%s]\r\n", snapshot.currentModeStr);
            printf("LoRa Frequency    : %lu Hz\r\n", (unsigned long)snapshot.frequencyHz);
            printf("LoRa Bandwidth    : %lu kHz\r\n", (unsigned long)snapshot.bandwidthKhz);
            printf("LoRa SpreadingFact: SF%u\r\n", snapshot.spreadingFactor);
            printf("LoRa Coding Rate  : 4/%u\r\n", snapshot.codingRate);
            printf("Packets Received  : %lu\r\n", (unsigned long)snapshot.totalPacketsReceived);
            printf("Packets Transmit  : %lu\r\n", (unsigned long)snapshot.totalPacketsTransmitted);
            printf("History In Memory : %zu Packets\r\n", snapshot.currentHistoryCount);
        } else {
            printf("LoRa Snapshot     : Error (LoraController Handle uninitialized)\r\n");
        }
    }
    else if (strncmp(cmd, "mute ", 5) == 0) {
        int parsed_time = atoi(cmd + 5);

        // FIXED: Expanded structural boundaries to explicitly accept 0 as a infinite lock parameter
        if (parsed_time >= 0 && parsed_time <= 3600) {
            current_mute_time_sec = static_cast<uint32_t>(parsed_time);

            if (current_mute_time_sec == 0) {
                // INFINITE MUTE: Security halt on the active FreeRTOS software timer instance
                if (silence_timer != nullptr) {
                    xTimerStop(silence_timer, pdMS_TO_TICKS(10));
                }
                printf("Success: Infinite mute activated. Debug logs are permanently suppressed.\r\n");
            } else {
                // DYNAMIC TIMEOUT: Extracted multiplication step to decouple constant values from structural macros
                uint32_t timeout_ms = current_mute_time_sec * 1000;

                // Dynamically re-trigger and alter operating parameters on the running software timer
                if (silence_timer != nullptr) {
                    xTimerChangePeriod(silence_timer, pdMS_TO_TICKS(timeout_ms), pdMS_TO_TICKS(10));
                }
                printf("Success: Mute timer interval updated to %lu seconds.\r\n", (unsigned long)current_mute_time_sec);
            }
        } else {
            printf("Error: Please provide a valid timeout between 0 (infinite) and 3600 seconds.\r\n");
        }
    }
    else if (strncmp(cmd, "blink ", 6) == 0) {
        uint32_t interval = 0;
        uint32_t pulse = 0;

        // FIXED: Swapped sscanf parsing tokens to %lu to eliminate narrowing warnings on signed integers
        if (sscanf(cmd + 6, "%lu %lu", &interval, &pulse) == 2) {
            if (interval > 0 && pulse > 0 && pulse < interval) {
                if (led_system != nullptr) {
                    LedController::BlinkProfile customProfile;
                    customProfile.intervalMs = interval;
                    customProfile.pulseWidthMs = pulse;

                    led_system->updateProfile(customProfile);
                    printf("Success: LED profile updated. Interval: %lu ms, Pulse: %lu ms.\r\n",
                        (unsigned long)interval, (unsigned long)pulse);
                } else {
                    printf("Error: LedController handle is uninitialized.\r\n");
                }
            } else {
                printf("Error: Invalid boundaries. Pulse width must be smaller than total interval.\r\n");
            }
        } else {
            printf("Usage error: Please specify parameters as 'blink <interval_ms> <pulse_width_ms>'\r\n");
        }
    }
    else if (strncmp(cmd, "freq ", 5) == 0) {
        uint32_t target_freq_hz = 0;
        if (sscanf(cmd + 5, "%lu", &target_freq_hz) == 1) {
            // Validate limits to protect the SX1276 front-end (e.g., 410 MHz to 525 MHz)
            bool valid_433 = (target_freq_hz >= 430000000 && target_freq_hz <= 440000000);
            bool valid_868 = (target_freq_hz >= 863000000 && target_freq_hz <= 870000000);

            if (valid_433 || valid_868) {
                if (lora_system != nullptr) {
                    lora_system->setFrequency(target_freq_hz);
                    printf("Success: Frequency shifted to %lu Hz.\r\n", (unsigned long)target_freq_hz);
                } else {
                    printf("Error: LoraController handle is uninitialized.\r\n");
                }
            } else {
                printf("Error: Frequency out of boundaries 430-440 / 863-870 MHz band limits.\r\n");
            }
        } else {
            printf("Usage error: Please specify parameters as 'freq <frequency_in_hz>'\r\n");
        }
    }
    else if (strncmp(cmd, "bw ", 3) == 0) {
        uint32_t target_bw = 0;
        if (sscanf(cmd + 3, "%lu", &target_bw) == 1) {
            if (target_bw == 62 || target_bw == 125 || target_bw == 250 || target_bw == 500) {
                if (lora_system != nullptr) {
                    lora_system->setBandwidth(target_bw);
                    printf("Success: Dispatched bandwidth configuration update to %lu kHz.\r\n", (unsigned long)target_bw);
                } else {
                    printf("Error: LoraController handle is uninitialized.\r\n");
                }
            } else {
                printf("Error: Invalid choice. Supported bandwidth options are: 62, 125, 250, 500.\r\n");
            }
        } else {
            printf("Usage error: Please specify parameter as 'bw <62|125|250|500>'\r\n");
        }
    }
    else if (strncmp(cmd, "oled ", 5) == 0) {
        uint32_t targetLine = 0;
        char textBuffer[64];

        // FIXED: Stabilized parsing definitions matching long unsigned integer casting requirements
        if (sscanf(cmd + 5, "%lu %63[^\r\n]", &targetLine, textBuffer) == 2) {
            if (targetLine <= 7) {
                if (oled_display != nullptr) {
                    oled_display->printMessage(static_cast<uint8_t>(targetLine), textBuffer);
                    printf("Success: Dispatched text '%s' to OLED line %lu.\r\n", textBuffer, (unsigned long)targetLine);
                } else {
                    printf("Error: DisplayController handle is uninitialized.\r\n");
                }
            } else {
                printf("Error: Target line must be within the range 0-7.\r\n");
            }
        } else {
            printf("Usage error: Please specify parameters as 'oled <line_id> <text_message>'\r\n");
        }
    }
    else if (strcmp(cmd, "i2c") == 0) {
        if (i2c_bus != nullptr) {
            printf("Scanning I2C Bus... \r\n");
            i2c_bus->scan();
        } else {
            printf("Error: I2C Bus not initialized in CLI!\r\n");
        }
    }
    else if (strcmp(cmd, "test-display") == 0) {
        printf("[CLI] Triggering asynchronous OLED font verification...\n");
        run_display_font_test(oled_display);
        printf("[CLI] Test completed.\n");
    }
    else {
        printf("Unknown command: '%s'. Type 'help' for options.\r\n", cmd);
    }

    printf("%s # ", Config::PROJECT_TITLE);
    fflush(stdout);
}

/**
 * @brief Infinite FreeRTOS task handling non-blocking background collection of serial input characters.
 * @param pvParameters Context input pointer passed by the task scheduler (unused).
 */
static void cli_worker_task(void *pvParameters) {
    (void)pvParameters;

    // Configure low-level peripheral settings for direct serial translation
    uart_config_t uart_config = {};
    uart_config.baud_rate     = 115200;
    uart_config.data_bits     = UART_DATA_8_BITS;
    uart_config.parity        = UART_PARITY_DISABLE;
    uart_config.stop_bits     = UART_STOP_BITS_1;
    uart_config.flow_ctrl     = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk    = UART_SCLK_DEFAULT;

    // Direct registration of system driver assets bypassing explicit peripheral queue maps
    ESP_ERROR_CHECK(uart_param_config(CLI_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(CLI_UART_PORT, BUF_SIZE * 2, 0, 0, nullptr, 0));

    char line_buffer[BUF_SIZE];
    int buffer_index = 0;

    printf("\r\n=== Interactive %s CLI Console Started ===\r\n", Config::PROJECT_TITLE);
    printf("Type 'help' to get a list of active instructions.\r\n\r\n");
    printf("%s # ", Config::PROJECT_TITLE);
    fflush(stdout);

    uint8_t data = 0;
    while (1) {
        // Evaluate input character frames utilizing a non-blocking asynchronous approach
        int len = uart_read_bytes(CLI_UART_PORT, &data, 1, pdMS_TO_TICKS(20));

        if (len > 0) {
            // Drop system verbosity parameters globally during active interaction sequence
            esp_log_level_set("*", ESP_LOG_NONE);

            // FIXED: Only reset the timer if we are not in permanent infinite mute mode (current_mute_time_sec != 0)
            if (silence_timer != nullptr && current_mute_time_sec != 0) {
                xTimerReset(silence_timer, pdMS_TO_TICKS(10));
            }

            // Capture Carriage Return or Line Feed sequence boundaries
            if (data == '\r' || data == '\n') {
                line_buffer[buffer_index] = '\0';
                process_command(line_buffer);
                buffer_index = 0;
            }
            // Evaluate Backspace command frames (ASCII 8 or Delete 127 symbols)
            else if ((data == 8 || data == 127) && buffer_index > 0) {
                buffer_index--;
                printf("\b \b");
                fflush(stdout);
            }
            // Sanitize and append terminal symbols (including UTF-8 multi-byte characters like German Umlaute)
            else if (((data >= 32 && data <= 126) || data >= 128) && buffer_index < (BUF_SIZE - 1)) {
                line_buffer[buffer_index++] = static_cast<char>(data);
                printf("%c", data);
                //fflush(stdout);
                // direct write into uart buffer, used for utf-8
                //uart_write_bytes(CLI_UART_PORT, &data, 1);
            }
        }
        // Structural task yields preventing internal stack watchdog violations
        vTaskDelay(pdMS_TO_TICKS(10));
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
        4096,
        nullptr,
        5,
        nullptr,
        0
    );
}
