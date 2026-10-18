
/**
 * @file config.hpp
 * @brief Global compile-time configuration profile and pin mapping for FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

// FIXED: Native C-library standard type injections to fix upstream ESP-IDF v6.1.0 picolibc header issues
// #include <sys/types.h>
// #include <sys/reent.h>
// #include <time.h>

#include <cstdint>
#include "driver/gpio.h"

namespace Config {
    // Compile-time constants with strict type enforcement
    constexpr const char* PROJECT_TITLE   = "FoxMesh";
    constexpr const char* PROJECT_VERSION = "1.0.5";
    constexpr const char* PROJECT_AUTHORS = "Tom OE3TKT";

    // CLI Mute Configuration
    constexpr uint32_t DEFAULT_MUTE_TIME_SEC = 30; // Logging suppression window in seconds

    // Hardware Configuration
    constexpr gpio_num_t LED_GPIO_PIN     = GPIO_NUM_25;
    constexpr int BLINK_INTERVAL_MS       = 5000;

    // OLED Display I2C Bus Configuration
    constexpr gpio_num_t OLED_SDA_PIN     = GPIO_NUM_21;
    constexpr gpio_num_t OLED_SCL_PIN     = GPIO_NUM_22;
    constexpr uint32_t OLED_I2C_FREQ_HZ   = 400000; // 400kHz Fast Mode execution speed
    constexpr int OLED_ADDRESS            = 0x3C;   // Native SSD1306 controller hardware address

    // Semtech SX1276 LoRa SPI Subsystem (TTGO T3_V1.6 Hardwired Layout)
    constexpr gpio_num_t LORA_MISO_PIN    = GPIO_NUM_19;
    constexpr gpio_num_t LORA_MOSI_PIN    = GPIO_NUM_27;
    constexpr gpio_num_t LORA_SCK_PIN     = GPIO_NUM_5;
    constexpr gpio_num_t LORA_CS_PIN      = GPIO_NUM_18;
    constexpr gpio_num_t LORA_RST_PIN     = GPIO_NUM_23; // Adjusted configuration for T3_V1.6 hardware revision
    constexpr gpio_num_t LORA_IRQ_PIN     = GPIO_NUM_26;

    // Semtech SX1276 LoRa Configuration Parameters
    constexpr uint32_t LORA_DEFAULT_FREQ_HZ = 433175000;
    constexpr uint32_t LORA_DEFAULT_BW_KHZ  = 250;
    constexpr uint8_t  LORA_DEFAULT_SF      = 11;
    constexpr uint8_t  LORA_DEFAULT_CR      = 6; // CR 4/6 -> Nenner ist 6

    // Der Compiler berechnet das 24-Bit Frequenzwort automatisch vor!
    constexpr uint64_t LORA_FRF_CALC      = ((uint64_t)LORA_DEFAULT_FREQ_HZ << 19) / 32000000ULL;
    constexpr uint8_t LORA_FRF_MSB        = static_cast<uint8_t>((LORA_FRF_CALC >> 16) & 0xFFU);
    constexpr uint8_t LORA_FRF_MID        = static_cast<uint8_t>((LORA_FRF_CALC >> 8) & 0xFFU);
    constexpr uint8_t LORA_FRF_LSB        = static_cast<uint8_t>(LORA_FRF_CALC & 0xFFU);

    // Berechne die Register-Hexwerte dynamisch basierend auf den obigen Angaben
    // BW: 250kHz -> 0x08. Shiften um 4 Bits nach links = 0x80. Coding Rate 4/6 -> 6-4 = 2 -> (2 << 1) = 0x04. Gesamt = 0x84.
    constexpr uint8_t LORA_MODEM_CONFIG_1 = 0x84;
    // SF11 -> 0x0B. Shiften um 4 Bits nach links = 0xB0. CRC On = 0x04. Gesamt = 0xB4.
    constexpr uint8_t LORA_MODEM_CONFIG_2 = 0xB4;

}
