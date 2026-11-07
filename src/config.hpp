
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
    // constexpr gpio_num_t LED_GPIO_PIN     = GPIO_NUM_25;
    // Hardware LED Configuration
    constexpr gpio_num_t LED_GPIO_PIN     = static_cast<gpio_num_t>(BOARD_LED_PIN);
    constexpr int BLINK_INTERVAL_MS       = 5000;

    // #define I2C1_SDA                    (42)
    // #define I2C1_SCL                    (41)
    // #define PMU_IRQ                     (40)
    // OLED Display I2C Bus Configuration
    constexpr gpio_num_t I2C_SDA_PIN   = static_cast<gpio_num_t>(BOARD_I2C_SDA);
    constexpr gpio_num_t I2C_SCL_PIN   = static_cast<gpio_num_t>(BOARD_I2C_SCL);
    constexpr uint32_t   I2C_FREQ_HZ   = 400000; // 400kHz Fast Mode execution speed
    constexpr gpio_num_t I2C1_SDA_PIN  = static_cast<gpio_num_t>(42);
    constexpr gpio_num_t I2C1_SCL_PIN  = static_cast<gpio_num_t>(41);
    constexpr uint32_t   I2C1_FREQ_HZ  = 400000; // 400kHz Fast Mode execution speed

    constexpr gpio_num_t PMU_IRQ       = static_cast<gpio_num_t>(40);
    constexpr int OLED_ADDRESS            = 0x3C;   // Native SSD1306 controller hardware address
    constexpr int PMU_ADDRESS             = 0x34;   // Native PMU AXP2101 controller hardware address

    // =========================================================================
    // TARGET PER-BOARD HARDWARE ROUTING & TRANSCEIVER LAYOUT
    // =========================================================================
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    // -------------------------------------------------------------------------
    // LilyGo T-Beam S3 Supreme (ESP32-S3) -> Semtech SX1262 Native Hardware
    // -------------------------------------------------------------------------
    constexpr gpio_num_t LORA_SCK_PIN     = static_cast<gpio_num_t>(BOARD_LORA_SCK);  // GPIO 12
    constexpr gpio_num_t LORA_MISO_PIN    = static_cast<gpio_num_t>(BOARD_LORA_MISO); // GPIO 13
    constexpr gpio_num_t LORA_MOSI_PIN    = static_cast<gpio_num_t>(BOARD_LORA_MOSI); // GPIO 11
    constexpr gpio_num_t LORA_CS_PIN      = static_cast<gpio_num_t>(BOARD_LORA_CS);   // GPIO 10
    constexpr gpio_num_t LORA_RST_PIN     = static_cast<gpio_num_t>(BOARD_LORA_RST);  // GPIO 5
    constexpr gpio_num_t LORA_BUSY_PIN    = static_cast<gpio_num_t>(BOARD_LORA_BUSY); // GPIO 4
    constexpr gpio_num_t LORA_DIO1_PIN    = static_cast<gpio_num_t>(BOARD_LORA_DIO1); // GPIO 1 (Primary IRQ)

    // LEGACY COMPATIBILITY ALIAS: Maps the old SX127x IRQ naming convention to DIO1
    // to keep your existing legacy driver code operational for multi-board compilation.
    constexpr gpio_num_t LORA_IRQ_PIN     = LORA_DIO1_PIN;

    // u-blox M10S GPS UART Mapping
    constexpr int GPS_UART_NUM            = 1;                                        // Use UART1 Controller
    constexpr gpio_num_t GPS_TX_PIN       = static_cast<gpio_num_t>(BOARD_GPS_TX);    // GPIO 8 (ESP RX)
    constexpr gpio_num_t GPS_RX_PIN       = static_cast<gpio_num_t>(BOARD_GPS_RX);    // GPIO 9 (ESP TX)
    constexpr gpio_num_t GPS_PPS_PIN      = static_cast<gpio_num_t>(BOARD_GPS_PPS);   // GPIO 6
    constexpr gpio_num_t GPS_EN_PIN       = static_cast<gpio_num_t>(BOARD_GPS_EN);    // GPIO 7
    constexpr uint32_t GPS_BAUD_RATE      = BOARD_GPS_BAUD;                           // 38400 Baud

#else
    // -------------------------------------------------------------------------
    // TTGO LoRa32 v2.1 (Classic ESP32) -> Semtech SX1276 Legacy Hardware
    // -------------------------------------------------------------------------
    constexpr gpio_num_t LORA_MISO_PIN    = GPIO_NUM_19;
    constexpr gpio_num_t LORA_MOSI_PIN    = GPIO_NUM_27;
    constexpr gpio_num_t LORA_SCK_PIN     = GPIO_NUM_5;
    constexpr gpio_num_t LORA_CS_PIN      = GPIO_NUM_18;
    constexpr gpio_num_t LORA_RST_PIN     = static_cast<gpio_num_t>(BOARD_LORA_RST);  // GPIO 23
    constexpr gpio_num_t LORA_IRQ_PIN     = GPIO_NUM_26;                              // Maps to DIO0
    constexpr gpio_num_t LORA_DIO1_PIN    = GPIO_NUM_33;                              // PLL Lock/Timeout

    // Legacy/No GPS Fallback Profile for classic TTGO
    constexpr int GPS_UART_NUM            = -1;
    constexpr gpio_num_t GPS_TX_PIN       = GPIO_NUM_NC;
    constexpr gpio_num_t GPS_RX_PIN       = GPIO_NUM_NC;
    constexpr gpio_num_t GPS_PPS_PIN      = GPIO_NUM_NC;
    constexpr gpio_num_t GPS_EN_PIN       = GPIO_NUM_NC;
    constexpr uint32_t GPS_BAUD_RATE      = 9600;
#endif

    // =========================================================================
    // LORA RF PARAMETERS & REGULATORY SETTINGS
    // =========================================================================

    // Semtech SX1276 LoRa Configuration Parameters
    // 433.175 MHz is calculated by: 433175000 / (32000000 / 2^19) = 7097139.2 = 0x6c4b33 // wrong: 0x06C4CD
    constexpr uint32_t LORA_DEFAULT_FREQ_HZ = 433175000;
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
    constexpr uint8_t LORA_SYNC_WORD = LORA_SYNC_WORD_PUBLIC_NETWORK_MODE;

    // Expand hardware preamble length to 12 symbols for robust network correlation (MeshCom value)
    // Expand hardware preamble length to 12 symbols for robust network correlation (MeshCom value)
    constexpr uint16_t LORA_PREAMBLE_LENGTH = 12; // FIXED: Changed uint18_t to correct uint16_t

    // Legacy SX127x Register Compatibility Array Flags (Will be bypassed by the future SX1262 driver packet structures)
    constexpr uint8_t  LORA_FRF_MSB         = 0x6C;
    constexpr uint8_t  LORA_FRF_MID         = 0x4B;
    constexpr uint8_t  LORA_FRF_LSB         = 0x33;
    constexpr uint8_t LORA_PREAMBLE_LENGTH_MSB = 0x00;
    constexpr uint8_t LORA_PREAMBLE_LENGTH_LSB = 0x0C;

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
