#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include "driver/gpio.h"
// #include "driver/i2c_master.h"

namespace Config {
    // Compile-time constants with strict type enforcement
    constexpr const char* PROJECT_TITLE   = "FoxMesh";
    constexpr const char* PROJECT_VERSION = "1.0.2";
    constexpr const char* PROJECT_AUTHORS = "Tom OE3TKT";

    // CLI Mute Configuration
    constexpr uint32_t DEFAULT_MUTE_TIME_SEC = 30; // Default: 30 seconds

    // Hardware Configuration
    constexpr gpio_num_t LED_GPIO_PIN     = GPIO_NUM_25;
    constexpr int BLINK_INTERVAL_MS       = 2000;
    // OLED
    constexpr gpio_num_t OLED_SDA_PIN     = GPIO_NUM_21;
    constexpr gpio_num_t OLED_SCL_PIN     = GPIO_NUM_22;
    constexpr uint32_t OLED_I2C_FREQ_HZ   = 400000; // 400kHz Fast Mode
    constexpr int OLED_ADDRESS            = 0x3C;   // Standard SSD1306 I2C Adress

    // Semtech SX1276 LoRa SPI Subsystem (TTGO T3_V1.6 Hardwired Layout)
    constexpr gpio_num_t LORA_MISO_PIN    = GPIO_NUM_19;
    constexpr gpio_num_t LORA_MOSI_PIN    = GPIO_NUM_27;
    constexpr gpio_num_t LORA_SCK_PIN     = GPIO_NUM_5;
    constexpr gpio_num_t LORA_CS_PIN      = GPIO_NUM_18;
    constexpr gpio_num_t LORA_RST_PIN     = GPIO_NUM_23; // Adjusted for T3_V1.6 hardware revision
    constexpr gpio_num_t LORA_IRQ_PIN     = GPIO_NUM_26;
}

#endif // CONFIG_HPP
