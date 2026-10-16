#include "cli.hpp"
#include <cstring>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "config.hpp"

static const char* TAG = "CLI";
#define CLI_UART_PORT UART_NUM_0
#define BUF_SIZE 1024

// Handle a silence timer for suppressing logs during cli actions
static TimerHandle_t silence_timer = nullptr;
// Dynamic variable holding the current mute runtime
static uint32_t current_mute_time_sec = Config::DEFAULT_MUTE_TIME_SEC;

static void log_restore_timer_callback(TimerHandle_t xTimer) {
    // Print a clean new prompt since logs might have disrupted the visual flow
    printf("\r\n[System] Debug logs unmuted after %lu seconds of inactivity. \r\n%s # ",
        (unsigned long)current_mute_time_sec, Config::PROJECT_TITLE);
    fflush(stdout);

    // Now safely restore global logging back to INFO level after inactivity
    esp_log_level_set("*", ESP_LOG_INFO);
}

// Process input strings once the user presses Enter
static void process_command(const char* cmd) {
    if (strlen(cmd) == 0) return;

    printf("\r\n"); // Move to new line after command execution

    if (strcmp(cmd, "help") == 0) {
        printf("Available commands:\r\n");
        printf("  help       - Show this command overview\r\n");
        printf("  version    - Display project title and version\r\n");
        printf("  status     - Print internal hardware statistics\r\n");
        printf("  mute <sec> - Set custom logging timeout (Current: %lus)\r\n", (unsigned long)current_mute_time_sec);
    }
    else if (strcmp(cmd, "version") == 0) {
        printf("%s System - Running Version: %s\r\n", Config::PROJECT_TITLE, Config::PROJECT_VERSION);
    }
    else if (strcmp(cmd, "status") == 0) {
        printf("--- %s Status ---\r\n", Config::PROJECT_TITLE);
        printf("Free Internal Heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
        printf("Mute Timer Period : %lu seconds\r\n", (unsigned long)current_mute_time_sec);
        printf("Target CPU Board  : ESP32\r\n");
    }
    // NEW: Handle the "mute" parameter runtime configuration
    else if (strncmp(cmd, "mute ", 5) == 0) {
        // Extract the value after the string space
        int new_time = atoi(cmd + 5);
        if (new_time > 0 && new_time <= 3600) { // Limit between 1 second and 1 hour
            current_mute_time_sec = new_time;

            // Instantly update the running FreeRTOS timer period
            if (silence_timer != nullptr) {
                xTimerChangePeriod(silence_timer, pdMS_TO_TICKS(current_mute_time_sec * 1000), pdMS_TO_TICKS(10));
            }
            printf("Success: Mute timer interval updated to %lu seconds.\r\n", (unsigned long)current_mute_time_sec);
        } else {
            printf("Error: Please provide a valid timeout between 1 and 3600 seconds.\r\n");
        }
    }
    else {
        printf("Unknown command: '%s'. Type 'help' for options.\r\n", cmd);
    }

    // Print command prompt again
    printf("\r\n%s # ", Config::PROJECT_TITLE);
    fflush(stdout);
}

// Background listening loop for keyboard input
static void cli_worker_task(void *pvParameters) {
    // Configure UART parameters for native monitoring
    uart_config_t uart_config = {};
    uart_config.baud_rate     = 115200;
    uart_config.data_bits     = UART_DATA_8_BITS;
    uart_config.parity        = UART_PARITY_DISABLE;
    uart_config.stop_bits     = UART_STOP_BITS_1;
    uart_config.flow_ctrl     = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk    = UART_SCLK_DEFAULT;

    // Install UART driver without using resource-heavy interrupt queues
    ESP_ERROR_CHECK(uart_param_config(CLI_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(CLI_UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));

    char line_buffer[BUF_SIZE];
    int buffer_index = 0;

    // Welcome message and initial prompt
    printf("\r\n=== Interactive %s CLI Console Started ===\r\n", Config::PROJECT_TITLE);
    printf("Type 'help' to get a list of active instructions.\r\n\r\n");
    printf("%s # ", Config::PROJECT_TITLE);
    fflush(stdout);

    uint8_t data;
    while (1) {
        // Read character by character from serial interface (non-blocking with 20ms timeout)
        int len = uart_read_bytes(CLI_UART_PORT, &data, 1, pdMS_TO_TICKS(20));

        if (len > 0) {
            esp_log_level_set("*", ESP_LOG_NONE);

            if (silence_timer != nullptr) {
                xTimerReset(silence_timer, pdMS_TO_TICKS(10));
            }
            // Handle carriage return or line feed (Enter key pressed)
            if (data == '\r' || data == '\n') {
                line_buffer[buffer_index] = '\0'; // Null-terminate string
                process_command(line_buffer);
                buffer_index = 0; // Reset buffer index for next command
            }
            // Handle Backspace (ASCII 8 or 127)
            else if ((data == 8 || data == 127) && buffer_index > 0) {
                buffer_index--;
                printf("\b \b"); // Clear character visually in user terminal
                fflush(stdout);
            }
            // Add normal printable characters to buffer
            else if (data >= 32 && data <= 126 && buffer_index < (BUF_SIZE - 1)) {
                line_buffer[buffer_index++] = data;
                printf("%c", data); // Terminal Echo (prints what user types)
                fflush(stdout);
            }
        }
        // Minimal delay to keep watchdog and FreeRTOS internal background tasks happy
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void start_cli_task(void) {
    // Dynamically initialize the timer with the variable interval using milliseconds conversion
    silence_timer = xTimerCreate(
        "MuteTimer",
        pdMS_TO_TICKS(current_mute_time_sec * 1000), 
        pdFALSE,              
        nullptr,
        log_restore_timer_callback
    );

    if (silence_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS silence timer.");
    }

    xTaskCreatePinnedToCore(
        cli_worker_task,
        "CLI_InterfaceTask",
        4096, // Slightly bigger stack to safely handle printf string buffers
        NULL,
        5,
        NULL,
        0 // Hosted on Core 0 to separate terminal interactive traffic from Core 1
    );
}
