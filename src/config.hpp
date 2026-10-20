
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
    constexpr const char* PROJECT_VERSION = "0.1";
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
    constexpr gpio_num_t LORA_DIO1_PIN    = GPIO_NUM_33; // Important: PLL lock & Timeout in RX mode

    // Semtech SX1276 LoRa Configuration Parameters
    // 433.175 MHz is calculated by: 433175000 / (32000000 / 2^19) = 7097139.2 = 0x6c4b33 // wrong: 0x06C4CD
    constexpr uint32_t LORA_DEFAULT_FREQ_HZ = 433175000;
    constexpr uint8_t  LORA_FRF_MSB         = 0x6C; // @TODO: will be removed in v1.1
    constexpr uint8_t  LORA_FRF_MID         = 0x4B; // @TODO: will be removed in v1.1
    constexpr uint8_t  LORA_FRF_LSB         = 0x33; // @TODO: will be removed in v1.1
    constexpr uint32_t LORA_DEFAULT_BW_KHZ  = 250;
    constexpr uint8_t  LORA_DEFAULT_SF      = 11;
    constexpr uint8_t  LORA_DEFAULT_CR      = 6;    // CR 4/6 -> Denominator is 6

    // Predefined LoRa Sync Word options for hardware correlation filtering, see docs/lora/device-settings.md
    constexpr uint8_t LORA_SYNC_WORD_PUBLIC_NETWORK_MODE   = 0x34; // MeshCom (ÖVSV standard for amateur radio mesh),
                                                                   // The Things Network, Helium
    constexpr uint8_t LORA_SYNC_WORD_TACTICAL_NETWORK_MODE = 0x24; // Meshtastic (Civilian off-grid tactical mesh network)
    constexpr uint8_t LORA_SYNC_WORD_PRIVATE_NETWORK_MODE  = 0x12; // Semtech Default (Factory default / Private point-to-point)
    constexpr uint8_t LORA_SYNC_WORD_ISOLATED_ARRAY_A      = 0x55; // Custom / Experimental individual networks (highly isolated)
    constexpr uint8_t LORA_SYNC_WORD_ISOLATED_ARRAY_B      = 0xAA; // Custom / Experimental individual networks (highly isolated)

    // Active hardware synchronization configuration selector
    constexpr uint8_t LORA_SYNC_WORD = LORA_SYNC_WORD_PUBLIC_NETWORK_MODE; // Selected default synchronization profile

    // Expand hardware preamble length to 12 symbols for robust network correlation (MeshCom value)
    constexpr uint8_t LORA_PREAMBLE_LENGTH_MSB = 0x00;
    constexpr uint8_t LORA_PREAMBLE_LENGTH_LSB = 0x0C; // Semtech Factor Default is 0x08, MeshCom with 0x0C, i.e. 12 symbols

    // --- DYNAMIC COMPILE-TIME BIT-SHIFTING EXPRESSIONS ---

    // Helper to map bandwidth kHz to Semtech Register Bits (7-4 of RegModemConfig1)
    constexpr uint8_t LORA_BW_REG_VAL = (LORA_DEFAULT_BW_KHZ == 62)  ? 0x06 :
                                        (LORA_DEFAULT_BW_KHZ == 125) ? 0x07 :
                                        (LORA_DEFAULT_BW_KHZ == 250) ? 0x08 :
                                        (LORA_DEFAULT_BW_KHZ == 500) ? 0x09 : 0x08;

    // RegModemConfig1 (0x1D): Bandwidth (Bits 7-4), Coding Rate (Bits 3-1), ImplicitHeader (Bit 0 = 0)
    constexpr uint8_t LORA_MODEM_CONFIG_1 = static_cast<uint8_t>((LORA_BW_REG_VAL << 4) | ((LORA_DEFAULT_CR - 4) << 1));

    // RegModemConfig2 (0x1E): Spreading Factor (Bits 7-4), TxContinuous (Bit 3 = 0), RxPayloadCrcOn (Bit 2 = 1)
    constexpr uint8_t LORA_MODEM_CONFIG_2 = static_cast<uint8_t>((LORA_DEFAULT_SF << 4) | 0x04U);
}
