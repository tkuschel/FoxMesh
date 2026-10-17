/**
 * @file display.cpp
 * @brief Thread-safe asynchronous SSD1306 OLED display driver implementation
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "display.hpp"
#include <cstring>
#include <cstdio>
#include "freertos/task.h"
#include "esp_log.h"
#include "config.hpp"

static const char* TAG = "DISPLAY";

// Standard 5x7 ASCII Font Table (covers characters from ASCII 32 ' ' to 126 '~')
// Each character consists of 5 vertical column bytes
// Extended with German Umlaute at the end: Ä (95), Ö (96), Ü (97), ä (98), ö (99), ü (100), ß (101)
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 Space
    {0x00, 0x00, 0x4f, 0x00, 0x00}, // 33 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // 35 #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // 36 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // 41 )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // 42 *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 48 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x32, 0x49, 0x79, 0x41, 0x3e}, // 64 @
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // 65 A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // 68 D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // 70 F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // 71 G
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // 72 H
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // 74 J
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // 77 M
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // 78 N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // 79 O
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // 81 Q
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 S
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // 84 T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // 85 U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // 86 V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x7f, 0x41, 0x41, 0x00}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 Backslash
    {0x00, 0x41, 0x41, 0x7f, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 a
    {0x7f, 0x48, 0x44, 0x44, 0x38}, // 98 b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 c
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // 100 d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 e
    {0x08, 0x7e, 0x09, 0x01, 0x02}, // 102 f
    {0x0c, 0x52, 0x52, 0x52, 0x3e}, // 103 g
    {0x7f, 0x08, 0x04, 0x04, 0x78}, // 104 h
    {0x00, 0x44, 0x7d, 0x40, 0x00}, // 105 i
    {0x20, 0x40, 0x44, 0x3d, 0x00}, // 106 j
    {0x7f, 0x10, 0x28, 0x44, 0x00}, // 107 k
    {0x00, 0x41, 0x7f, 0x40, 0x00}, // 108 l
    {0x7c, 0x04, 0x18, 0x04, 0x78}, // 109 m
    {0x7c, 0x08, 0x04, 0x04, 0x78}, // 110 n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 o
    {0x7c, 0x14, 0x14, 0x14, 0x08}, // 112 p
    {0x08, 0x14, 0x14, 0x18, 0x7c}, // 113 q
    {0x7c, 0x08, 0x04, 0x04, 0x08}, // 114 r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 s
    {0x04, 0x3f, 0x44, 0x40, 0x20}, // 116 t
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // 117 u
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, // 118 v
    {0x3c, 0x40, 0x30, 0x40, 0x3c}, // 119 w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 x
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, // 121 y
    {0x44, 0x64, 0x54, 0x4c, 0x44}, // 122 z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 \{
    {0x00, 0x00, 0x77, 0x00, 0x00}, // 124 |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 \}
    {0x08, 0x08, 0x2a, 0x1c, 0x08}, // 126 ~
    // German Umlaute mappings
    {0x7d, 0x11, 0x11, 0x11, 0x7d}, // 127 Ä
    {0x3c, 0x41, 0x41, 0x41, 0x3c}, // 128 Ö
    {0x3d, 0x40, 0x40, 0x40, 0x3d}, // 129 Ü
    {0x20, 0x55, 0x54, 0x54, 0x78}, // 130 ä
    {0x38, 0x54, 0x54, 0x54, 0x38}, // 131 ö
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // 132 ü
    {0x7f, 0x49, 0x49, 0x36, 0x20}  // 133 ß
};

/**
 * @brief Construct a new Display Controller object and register the device on the I2C bus.
 * @param bus_pointer Pointer to the active, thread-safe hardware I2C master bus instance.
 */
DisplayController::DisplayController(i2c* bus_pointer) : m_bus(bus_pointer) {
    if (m_bus == nullptr) {
        ESP_LOGE(TAG, "Failed creation: Input I2C bus pointer context is null!");
        return;
    }

    // Configure the specific device attachment parameters tracking your SSD1306 layout
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = static_cast<uint16_t>(Config::OLED_ADDRESS);
    dev_config.scl_speed_hz = Config::OLED_I2C_FREQ_HZ;

    // Safely attach the display device context onto OM Tom's infrastructure bus handle
    esp_err_t err = i2c_master_bus_add_device(m_bus->get_bus_handle(), &dev_config, &m_devHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register SSD1306 on the I2C Master Bus: %s", esp_err_to_name(err));
        m_devHandle = nullptr;
    }
}

/**
 * @brief Destroy the Display Controller object, freeing up allocated queue and device resources.
 */
DisplayController::~DisplayController() {
    if (m_devHandle != nullptr) {
        i2c_master_bus_rm_device(m_devHandle);
    }
    if (m_msgQueue != nullptr) {
        vQueueDelete(m_msgQueue);
    }
}

/**
 * @brief Internal helper to dispatch low-overhead raw hardware command sequences to the display.
 * @param cmd The raw execution command configuration byte.
 * @return esp_err_t status metric indicating whether the transfer succeeded.
 */
esp_err_t DisplayController::sendCommand(uint8_t cmd) const {
    if (m_devHandle == nullptr) return ESP_ERR_INVALID_STATE;

    // SSD1306 command stream control stream frame format payload byte array
    uint8_t buffer[2] = { 0x00, cmd };
    return i2c_master_transmit(m_devHandle, buffer, sizeof(buffer), 100);
}

/**
 * @brief Triggers the fundamental register setup and executes a ultra-reliable high-speed screen wipe.
 * @return esp_err_t status tracking variable collecting transaction anomalies.
 */
esp_err_t DisplayController::initializeHardware() {
    if (m_devHandle == nullptr) return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;

    // Ultra-reliable fundamental minimal initialization sequence configuration steps
    err |= sendCommand(0xAE); // Turn display off
    err |= sendCommand(0xD5); // Set clock divide ratio
    err |= sendCommand(0x80);
    err |= sendCommand(0xA8); // Set multiplex ratio
    err |= sendCommand(0x3F); // 1/64 duty cycle setup
    err |= sendCommand(0xD3); // Set display offset
    err |= sendCommand(0x00); // No offset
    err |= sendCommand(0x40); // Set start line at 0
    err |= sendCommand(0x8D); // Charge pump configuration setup
    err |= sendCommand(0x14); // Enable charge pump power rails
    err |= sendCommand(0x20); // Set memory addressing mode profile
    err |= sendCommand(0x02); // Page addressing mode (highly predictable translation)
    err |= sendCommand(0xA1); // Segment re-map columns configuration matrix
    err |= sendCommand(0xC8); // COM Output scan direction setting execution
    err |= sendCommand(0xDA); // COM pins hardware structural layout profile
    err |= sendCommand(0x12);
    err |= sendCommand(0x81); // Contrast configuration mapping setting
    err |= sendCommand(0x7F);
    err |= sendCommand(0xA4); // Resume display to RAM mapping entries
    err |= sendCommand(0xA6); // Normal display output formatting selection profile

    // High-Speed Screen Clear on startup to guarantee zero RAM noise artifacts
    for (uint8_t page = 0; page < 8; page++) {
        err |= sendCommand(static_cast<uint8_t>(0xB0 + page)); // Set page boundary
        err |= sendCommand(0x00);                              // Reset lower column pointer
        err |= sendCommand(0x10);                              // Reset higher column pointer

        // Allocate high-speed contiguous flash transmission block buffer (1 control byte + 128 data bytes)
        uint8_t clearBuffer[129] = {0};
        clearBuffer[0] = 0x40; // Enforce explicit Data Mode streaming identifier token

        // The remaining 128 bytes are already natively initialized to 0x00 via the aggregate initializer {0}
        if (m_devHandle != nullptr) {
            err |= i2c_master_transmit(m_devHandle, clearBuffer, sizeof(clearBuffer), 50);
        }
    }

    err |= sendCommand(0xAF); // Wake up physical controller panel (display ON)

    return err;
}

/**
 * @brief Instantiates the static FreeRTOS messaging pipeline and deploys the background consumer thread onto Core 0.
 * @param priority Task priority rank assignment metric handled by the scheduler.
 */
void DisplayController::startTask(uint32_t priority) {
    // Allocate the static thread-safe communication data path
    m_msgQueue = xQueueCreate(8, sizeof(DisplayMessage));
    if (m_msgQueue == nullptr) {
        ESP_LOGE(TAG, "Critical error: Could not allocate memory for display communication queue!");
        return;
    }

    // Deploy onto Core 0 alongside the terminal pipeline to keep Core 1 free for LoRa scheduling
    xTaskCreatePinnedToCore(
        DisplayController::taskTrampoline,
        "OLED_WorkerTask",
        4096,
        this,
        priority,
        nullptr,
        0
    );
}
/**
 * @brief Safe cross-core interface dispatching a text block onto the internal processing pipeline buffer.
 * @param text Pointer to a standard null-terminated sequence of tracking information.
 * @param line Selected hardware page index mapping from 0 to 7.
 */
void DisplayController::printMessage(const char* text, uint8_t line) {
    if (m_msgQueue == nullptr || text == nullptr) return;

    DisplayMessage msg = {};
    msg.targetLine = line;

    // Thread-safe copy buffer sanitization sequence
    strncpy(msg.content, text, sizeof(msg.content) - 1);
    msg.content[sizeof(msg.content) - 1] = '\0';

    // Asynchronous non-blocking dispatch push (bails instantly if full to prevent lockups)
    xQueueSend(m_msgQueue, &msg, 0);
}

/**
 * @brief Static trampoline layer connecting the native C-based FreeRTOS scheduler directly back to our C++ runtime context.
 * @param pvParameters Raw pointer mapping the reference context of the current instance tracking space.
 */
void DisplayController::taskTrampoline(void* pvParameters) {
    auto* instance = static_cast<DisplayController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }
    vTaskDelete(nullptr);
}

/**
 * @brief Autonomous main task routing layer parsing compound UTF-8 string inputs and enforcing real-time automatic end-of-line wiping.
 */
void DisplayController::run() {
    // Perform hardware initialization safely encapsulated inside the unique thread sequence boundary
    esp_err_t init_status = initializeHardware();
    if (init_status != ESP_OK) {
        ESP_LOGE(TAG, "Hardware setup failed completely. Execution layer aborted.");
        return;
    }

    ESP_LOGI(TAG, "SSD1306 display initialized and listening for incoming render packages.");

    DisplayMessage incoming = {};

    while (true) {
        // Asynchronous structural block yield: consumes zero CPU until a new message package arrives
        if (xQueueReceive(m_msgQueue, &incoming, portMAX_DELAY) == pdTRUE) {
            if (incoming.targetLine > 7) incoming.targetLine = 7;

            // Positioning the internal hardware pointer inside the target page boundary
            sendCommand(static_cast<uint8_t>(0xB0 + incoming.targetLine)); // Set page address boundary
            sendCommand(0x00);                                            // Set lower column pointer
            sendCommand(0x10);                                            // Set higher column pointer
            uint8_t totalBytesWritten = 0;

            // Basic built-in 5x7 ASCII micro character font generation layer
            // This prevents adding bloating third party render dependencies into your tight stack space
            size_t textLength = strlen(incoming.content);

            for (size_t i = 0; i < textLength; i++) {
                uint8_t c_primary = static_cast<uint8_t>(incoming.content[i]);
                size_t fontIndex = 0;
                bool validCharacter = false;

                // Basic UTF-8 multi-byte decoder for native German Umlaute translation
                if (c_primary == 0xC3 && (i + 1) < textLength) {
                    uint8_t c_secondary = static_cast<uint8_t>(incoming.content[i + 1]);
                    i++; // Advance the pointer to consume the compound UTF-8 token pair

                    if (c_secondary == 0x84) { fontIndex = 95;  validCharacter = true; } // Ä
                    if (c_secondary == 0x96) { fontIndex = 96;  validCharacter = true; } // Ö
                    if (c_secondary == 0x9C) { fontIndex = 97;  validCharacter = true; } // Ü
                    if (c_secondary == 0xA4) { fontIndex = 98;  validCharacter = true; } // ä
                    if (c_secondary == 0xB6) { fontIndex = 99;  validCharacter = true; } // ö
                    if (c_secondary == 0xBC) { fontIndex = 100; validCharacter = true; } // ü
                }
                else if (c_primary == 0xC2 && (i + 1) < textLength) {
                    uint8_t c_secondary = static_cast<uint8_t>(incoming.content[i + 1]);
                    i++;
                    if (c_secondary == 0x9F) { fontIndex = 101; validCharacter = true; } // ß
                }
                else if (c_primary >= 32 && c_primary <= 126) {
                    fontIndex = static_cast<size_t>(c_primary - 32);
                    validCharacter = true;
                }

                if (validCharacter && totalBytesWritten < 122) {
                    uint8_t txBuffer[7] = {0};
                    txBuffer[0] = 0x40; // Data identifier

                    txBuffer[1] = font_5x7[fontIndex][0];
                    txBuffer[2] = font_5x7[fontIndex][1];
                    txBuffer[3] = font_5x7[fontIndex][2];
                    txBuffer[4] = font_5x7[fontIndex][3];
                    txBuffer[5] = font_5x7[fontIndex][4];
                    txBuffer[6] = 0x00; // 1-pixel spacing between characters

                    if (m_devHandle != nullptr) {
                        i2c_master_transmit(m_devHandle, txBuffer, sizeof(txBuffer), 50);
                    }
                    totalBytesWritten = static_cast<uint8_t>(totalBytesWritten + 6);
                }
            }

            // AUTO-CLEAR: Fill the remaining horizontal page width with blank pixels to flush out trailing leftovers
            if (totalBytesWritten < 128) {
                uint8_t remainingBytes = static_cast<uint8_t>(128 - totalBytesWritten);

                // Allocate a burst buffer with dynamic sizing plus control byte room
                uint8_t flushBuffer[129] = {0};
                flushBuffer[0] = 0x40; // Data mode identifier

                if (m_devHandle != nullptr) {
                    // Send out the precise amount of zeros required to wipe the right margin clean
                    i2c_master_transmit(m_devHandle, flushBuffer, static_cast<size_t>(remainingBytes + 1), 50);
                }
            }
        }
    }
}
