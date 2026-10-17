/**
 * @file lora.cpp
 * @brief Thread-safe asynchronous Semtech SX1276 LoRa transceiver driver implementation
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "lora.hpp"
#include "esp_log.h"
#include "config.hpp"
#include "display.hpp"
#include <cstring>

const char* LoraController::TAG = "LORA";

LoraController::LoraController() {
    m_isrSemaphore = xSemaphoreCreateBinary();
    if (m_isrSemaphore == nullptr) {
        ESP_LOGE(TAG, "Fatal: Failed to allocate LoRa ISR synchronization semaphore.");
    }
}

LoraController::~LoraController() {
    if (m_spiHandle != nullptr) {
        spi_bus_remove_device(m_spiHandle);
    }
    if (m_isrSemaphore != nullptr) {
        vSemaphoreDelete(m_isrSemaphore);
    }
}

uint8_t LoraController::readRegister(uint8_t address) {
    if (m_spiHandle == nullptr) return 0;

    uint8_t txData[2] = { static_cast<uint8_t>(address & 0x7FU), 0x00U };
    uint8_t rxData[2] = { 0x00U, 0x00U };

    spi_transaction_t trans = {};
    trans.length = 16;
    trans.tx_buffer = txData;
    trans.rx_buffer = rxData;

    if (spi_device_transmit(m_spiHandle, &trans) != ESP_OK) {
        return 0;
    }
    return rxData[1];
}

void LoraController::writeRegister(uint8_t address, uint8_t value) {
    if (m_spiHandle == nullptr) return;

    uint8_t txData[2] = { static_cast<uint8_t>(address | 0x80U), value };

    spi_transaction_t trans = {};
    trans.length = 16;
    trans.tx_buffer = txData;

    spi_device_transmit(m_spiHandle, &trans);
}

void IRAM_ATTR LoraController::gpioIsrHandler(void* arg) {
    auto* semaphore = static_cast<SemaphoreHandle_t>(arg);
    if (semaphore != nullptr) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(semaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

esp_err_t LoraController::initializeHardware() {
    esp_err_t err;

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 9000000;
    devcfg.mode           = 0;
    devcfg.spics_io_num   = Config::LORA_CS_PIN;
    devcfg.queue_size     = 7;

    err = spi_bus_add_device(SPI3_HOST, &devcfg, &m_spiHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach device handle to SPI3 host: %s", esp_err_to_name(err));
        return err;
    }

    gpio_reset_pin(Config::LORA_RST_PIN);
    gpio_set_direction(Config::LORA_RST_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(Config::LORA_IRQ_PIN);
    gpio_set_direction(Config::LORA_IRQ_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(Config::LORA_IRQ_PIN, GPIO_PULLDOWN_ONLY);

    gpio_set_level(Config::LORA_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(Config::LORA_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t version = readRegister(0x42);
    if (version != 0x12) {
        ESP_LOGE(TAG, "Transceiver handshake failed! Invalid signature: 0x%02X", version);
        return ESP_ERR_INVALID_VERSION;
    }
    ESP_LOGI(TAG, "SX1276 Handshake success. Silicon version confirmed: 0x12");

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = gpio_set_intr_type(Config::LORA_IRQ_PIN, GPIO_INTR_POSEDGE);
    err = gpio_isr_handler_add(Config::LORA_IRQ_PIN, LoraController::gpioIsrHandler, m_isrSemaphore);

    return err;
}

void LoraController::setModeReceiver() {
    writeRegister(0x01, 0x80U); // Sleep + LoRa Mode
    vTaskDelay(pdMS_TO_TICKS(10));

    // Hardwired for 868.1 MHz fallback channel
    writeRegister(0x06, 0xD9U);
    writeRegister(0x07, 0x06U);
    writeRegister(0x08, 0x66U);

    writeRegister(0x40, 0x00U); // DIO0 map onto RxDone
    writeRegister(0x01, 0x85U); // RXCONTINUOUS
    ESP_LOGI(TAG, "Radio base configuration applied. Listening on 868.1 MHz.");
}

esp_err_t LoraController::transmitFrame(const uint8_t* buffer, uint8_t length) {
    if (m_spiHandle == nullptr || buffer == nullptr || length == 0) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Initiating transmission sequence: Sending %u bytes.", length);
    writeRegister(0x01, 0x81U); // STDBY Mode
    writeRegister(0x0D, 0x00U); // RegFifoTxBaseAddr
    writeRegister(0x09, 0x00U); // RegFifoAddrPtr

    for (uint8_t i = 0; i < length; i++) {
        writeRegister(0x00, buffer[i]);
    }

    writeRegister(0x22, length); // RegPayloadLength
    writeRegister(0x01, 0x83U);  // RegOpMode: TX Mode Trigger

    return ESP_OK;
}

void LoraController::taskTrampoline(void* pvParameters) {
    auto* instance = static_cast<LoraController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }
    vTaskDelete(nullptr);
}

void LoraController::run() {
    if (initializeHardware() != ESP_OK) {
        ESP_LOGE(TAG, "Critical: LoRa init failed. Thread aborted.");
        return;
    }

    setModeReceiver();
    extern DisplayController* display; // Reference the global display singleton

    while (true) {
        if (xSemaphoreTake(m_isrSemaphore, portMAX_DELAY) == pdTRUE) {
            uint32_t irqFlags = readRegister(0x12);
            writeRegister(0x12, static_cast<uint8_t>(irqFlags));

            if ((irqFlags & 0x40U) != 0U) { // RxDone
                LoraPacket packet = {};
                packet.length = readRegister(0x13);

                uint32_t currentFifoAddress = readRegister(0x10);
                writeRegister(0x09, static_cast<uint8_t>(currentFifoAddress));

                for (uint8_t i = 0; i < packet.length; i++) {
                    packet.payload[i] = readRegister(0x00);
                }

                packet.rssi = static_cast<int8_t>(static_cast<int>(readRegister(0x1A)) - 157);
                packet.snr  = static_cast<int8_t>(static_cast<int>(readRegister(0x1B)) / 4);

                ESP_LOGI(TAG, "Incoming frame packet received! Size: %u bytes, RSSI: %d dBm, SNR: %d dB",
                    packet.length, packet.rssi, packet.snr);

                // FIXED: Uses the new parameter order (line first, text second)
                if (display != nullptr) {
                    display->printMessage(1, "LoRa Msg Received");
                }
            }
        }
    }
}

void LoraController::startTask(uint32_t priority) {
    xTaskCreatePinnedToCore(
        LoraController::taskTrampoline,
        "LORA_EngineTask",
        4096,
        this,
        priority,
        &m_taskHandle,
        1
    );
}
