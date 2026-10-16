#include "display.hpp"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "config.hpp"
#include <cstring>

const char* DisplayController::TAG = "DISPLAY";

static i2c_master_bus_handle_t bus_handle = nullptr;
static i2c_master_dev_handle_t dev_handle = nullptr;

DisplayController::DisplayController() : m_msgQueue(nullptr) {}

void DisplayController::initI2C() {
    // Configure the physical I2C Bus first
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = Config::OLED_SDA_PIN;
    bus_config.scl_io_num = Config::OLED_SCL_PIN;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true; // use internal pull-up resistor

    // ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(err));
        return;
    }

    // Add the specific SSD1306 OLED Device to the running bus
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = Config::OLED_ADDRESS;
    dev_config.scl_speed_hz = Config::OLED_I2C_FREQ_HZ;

    err = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add OLED device to I2C bus: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "New I2C Master Bus and Device Handle registered successfully.");
}


void DisplayController::sendCommand(uint8_t command) {
    if (dev_handle == nullptr) return;

    // the new driver handles transactions internally in a single, safe array write
    uint8_t write_buf[2] = {
        0x00,   // Control byte: Co=0, D/C=0 (Incoming bytes are commands)
        command // The actual payload command
    };

    // Increased timeout to 50ms and replaced hard ESP_ERROR_CHECK with a safe warning log
    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(50));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OLED Command 0x%02X failed or timed out: %s", command, esp_err_to_name(ret));
    }
}

void DisplayController::initSSD1306() {
    // If the hardware setup failed, don't run commands against a null handle
    if (dev_handle == nullptr) {
        ESP_LOGE(TAG, "Skipping SSD1306 init sequence because dev_handle is null.");
        return;
    }
    // Minimalistic SSD1306 initialization sequence for 128x64 OLEDs
    sendCommand(0xAE); // Turn display off
    sendCommand(0xD5); // Set display clock divide ratio
    sendCommand(0x80);
    sendCommand(0xA8); // Set multiplex ratio
    sendCommand(0x3F); // 64MUX
    sendCommand(0xD3); // Set display offset
    sendCommand(0x00);
    sendCommand(0x40); // Set display start line
    sendCommand(0x8D); // Charge pump charge
    sendCommand(0x14); // Enable charge pump
    sendCommand(0xAF); // Turn display on

    // clear screen via modern buffer transfer
     uint8_t clear_buf[129];
    clear_buf[0] = 0x40; // Control byte: Data stream follows
    memset(&clear_buf[1], 0x00, 128); // Fill frame buffer slice with zeros

    // Clear screen natively by setting page addressing
    for (uint8_t page = 0; page < 8; page++) {
        sendCommand(0xB0 + page); // Set page address
        sendCommand(0x00);        // Set lower column address
        sendCommand(0x10);        // Set higher column address

        // FIXED: Replaced ESP_ERROR_CHECK with safe error-handling to prevent software panic crashes
        esp_err_t ret = i2c_master_transmit(dev_handle, clear_buf, sizeof(clear_buf), pdMS_TO_TICKS(50));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear page %d buffer: %s. Aborting sequence.", page, esp_err_to_name(ret));
            return; // Exit cleanly, allowing the CPU to boot other tasks
        }
    }
    ESP_LOGI(TAG, "SSD1306 OLED Controller initialized and screen cleared.");
}

void DisplayController::startTask(uint32_t priority) {
    initI2C();
    initSSD1306();

    // Create the message queue for up to 10 incoming text packages
    m_msgQueue = xQueueCreate(10, sizeof(DisplayMessage));

    // Spawn the worker thread on Core 0 (keeps Core 1 completely free for your Mesh engine)
    xTaskCreatePinnedToCore(
        DisplayController::taskTrampoline,
        "OLED_RenderTask",
        3584,
        this,
        priority,
        nullptr,
        0 // Pinned to Core 0
    );
}

bool DisplayController::printMessage(const char* text, int row) {
    if (m_msgQueue == nullptr) return false;

    DisplayMessage msg = {};
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.row = row;

    // Non-blocking write into the queue from any external task thread
    return (xQueueSend(m_msgQueue, &msg, pdMS_TO_TICKS(10)) == pdPASS);
}

void DisplayController::taskTrampoline(void* pvParameters) {
    auto* instance = static_cast<DisplayController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }
    vTaskDelete(nullptr);
}

void DisplayController::run() {
    DisplayMessage incomingMsg;
    ESP_LOGI(TAG, "OLED Core Engine operational loop active.");

    // Dynamic boot screen utilizing configuration metadata
    printMessage("====================", 0); // Row 0: Top Frame

    // Format the title and version cleanly into Row 1
    char titleBuffer[32];
    snprintf(titleBuffer, sizeof(titleBuffer), "%s v%s", Config::PROJECT_TITLE, Config::PROJECT_VERSION);
    printMessage(titleBuffer, 1);

    // Format the copyright and authors into Row 2
    char authorBuffer[32];
    snprintf(authorBuffer, sizeof(authorBuffer), "(c) %s", Config::PROJECT_AUTHORS);
    printMessage(authorBuffer, 2);

    printMessage("====================", 3); // Row 3: Bottom Frame

    while (true) {
        // Deep sleep state: Task hits 0% CPU usage until a message arrives in the queue
        if (xQueueReceive(m_msgQueue, &incomingMsg, portMAX_DELAY) == pdPASS) {
            // Safe instrumentation logging (simulating the physical pixels)
            ESP_LOGI(TAG, "[OLED ROW %d]: %s", incomingMsg.row, incomingMsg.text);
        }
    }
}