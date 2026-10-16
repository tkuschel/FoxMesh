#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "config.hpp"
#include "led.hpp"
#include "cli.hpp"
#include "display.hpp"  // thread-safe display

static const char *TAG = "MAIN";

// Global singleton instances for hardware abstraction
static DisplayController oled;
static LedController led(Config::LED_GPIO_PIN);

void init_lora_spi(void) {
    esp_err_t ret;

    // Configure the SPI structure using the strict C++ constexpr constants
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = Config::LORA_MOSI_PIN;
    buscfg.miso_io_num = Config::LORA_MISO_PIN;
    buscfg.sclk_io_num = Config::LORA_SCK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;

    // Initialize the physical peripheral (VSPI / SPI3 on ESP32)
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI bus initialized successfully on VSPI (SPI3)");
}

// Fixed: Only ONE app_main exists now, marked as extern "C" for ESP-IDF bootloader
extern "C" void app_main(void) {
    // Dynamic string formatting using type-safe constants
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, " Booting %s (Version: %s) via ESP-IDF C++",
            Config::PROJECT_TITLE,
            Config::PROJECT_VERSION);
    ESP_LOGI(TAG, " (c) Authors: %s", Config::PROJECT_AUTHORS);
    ESP_LOGI(TAG, "=================================================");

    // Explicitly power up the onboard peripherals on TTGO T3
//    gpio_reset_pin(GPIO_NUM_16); 
//    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    // Hard reset of the OLED display
//    gpio_set_level(GPIO_NUM_16, 0);
 //   vTaskDelay(pdMS_TO_TICKS(50));  // 50ms in Reset

//    gpio_set_level(GPIO_NUM_16, 1); // Set HIGH to enable OLED power rails
//    vTaskDelay(pdMS_TO_TICKS(100));  // Give the hardware 100ms to stabilize voltage

    led.startTask();

    start_cli_task();

    oled.startTask();


    // Synchronous test push to the display queue
//    oled.printMessage("Mesh Scanning...", 2);

    // Sytem Monitor Loop
    while (true) {
        ESP_LOGI(TAG, "[%s v%s] Operational. Free Heap: %lu bytes",
                 Config::PROJECT_TITLE,
                 Config::PROJECT_VERSION,
                 (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
