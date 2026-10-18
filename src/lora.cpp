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

/**
 * @brief Constructor initializing core FreeRTOS synchronization primitives.
 */
LoraController::LoraController() {
    m_isrSemaphore = xSemaphoreCreateBinary();
    m_stateMutex = xSemaphoreCreateMutex();

    if (m_isrSemaphore == nullptr || m_stateMutex == nullptr) {
        ESP_LOGE(TAG, "Fatal: Failed to allocate LoRa synchronization primitives.");
    }
}

/**
 * @brief Destructor handling clean resource deallocation and device detaching.
 */
LoraController::~LoraController() {
    if (m_spiHandle != nullptr) {
        // Remove device hook from SPI stack registry
    }
    if (m_isrSemaphore != nullptr) {
        vSemaphoreDelete(m_isrSemaphore);
    }
    if (m_stateMutex != nullptr) {
        vSemaphoreDelete(m_stateMutex);
    }
}

/**
 * @brief Low-level driver method to extract a byte value from a transceiver configuration register.
 * @param address The hardware register memory space tracking coordinate byte.
 * @return The data register value content read back via the SPI transactions stack.
 */
uint8_t LoraController::readRegister(uint8_t address) {
    // Hardware abstraction implementation...
    return 0;
}

/**
 * @brief Low-level driver method to override an option byte parameter inside a transceiver hardware register.
 * @param address The hardware register memory space tracking coordinate byte.
 * @param value The option layout bits context to be committed directly into the transceiver.
 */
void LoraController::writeRegister(uint8_t address, uint8_t value) {
    // Hardware abstraction implementation...
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
/**
 * @brief Static hardware interrupt service module routing fast signal state shifts back into thread flags.
 * @param arg Context pointer referencing our runtime binary execution thread bridge semaphore.
 */
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
#pragma GCC diagnostic pop

/**
 * @brief Orchestrates base interface configurations, system pin assignments, and checks silicon identities.
 * @return ESP_OK upon passing functional signature checks, alternative error frameworks upon link drops.
 */
esp_err_t LoraController::initializeHardware() {
    // Initializations steps omitted for length consistency...
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.isInitialized = true;
        xSemaphoreGive(m_stateMutex);
    }
    return ESP_OK;
}

/**
 * @brief Drives internal radio operational states back into endless listening configurations.
 */
void LoraController::setModeReceiver() {
    // Basic setup configurations routing context...
}

/**
 * @brief Modifies operational carrier targets and maps execution metrics thread-safely.
 * @param freq_hz Core tuning performance frequency specified in absolute Hertz.
 */
void LoraController::setFrequency(uint32_t freq_hz) {
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.frequencyHz = freq_hz;

        // Translate frequency configuration parameter layouts to register bytes...
        // writeRegister(0x06, msb); etc.

        xSemaphoreGive(m_stateMutex);
        ESP_LOGI(TAG, "Frequency updated to %lu Hz", (unsigned long)freq_hz);
    }
}

/**
 * @brief Recovers active runtime radio target frequencies thread-safely.
 * @return The base operational RF channel configured in absolute Hertz scale.
 */
uint32_t LoraController::getFrequency() {
    uint32_t freq = 0;
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        freq = m_state.frequencyHz;
        xSemaphoreGive(m_stateMutex);
    }
    return freq;
}

/**
 * @brief Overrides the operational hardware modulation grid space parameter configuration.
 * @param bw_khz Operating modulation channel grid width metric parameters.
 */
void LoraController::setBandwidth(uint32_t bw_khz) {
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.bandwidthKhz = bw_khz;
        // Apply modem updates...
        xSemaphoreGive(m_stateMutex);
    }
}

/**
 * @brief Computes high-precision calibrated RSSI and SNR configurations based on frequency bands.
 * @note Now uses high-precision float conversions to preserve the full 0.25 dB native hardware steps.
 * @param rawRssi Unprocessed signal strength readout byte straight from RegRssiValue (0x1A).
 * @param rawSnr Unprocessed signed signal-to-noise readout byte from RegPktSnrValue (0x1B).
 * @param[out] outRssi Reference capturing the floating-point calibrated signal power in dBm.
 * @param[out] outSnr Reference capturing the floating-point calibrated signal-to-noise ratio in dB.
 */
void LoraController::calculateTelemetry(uint8_t rawRssi, uint8_t rawSnr, float& outRssi, float& outSnr) {
    uint32_t currentFreq = 0;

    // Securely pull the active runtime frequency configuration
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        currentFreq = m_state.frequencyHz;
        xSemaphoreGive(m_stateMutex);
    }

    // 1. Precise SNR calculation (preserve native 0.25 dB resolution step by step)
    float actualSnr = static_cast<float>(static_cast<int8_t>(rawSnr)) * 0.25f;
    outSnr = actualSnr;

    // 2. Select calibrated RSSI Offset base (Low-Band <= 525MHz vs High-Band > 525MHz)
    float rssiOffset = (currentFreq <= 525000000) ? -164.0f : -157.0f;

    // 3. Compute accurate RSSI factoring sub-noise floor channel regressions
    if (actualSnr < 0.0f) {
        outRssi = rssiOffset + static_cast<float>(rawRssi) + actualSnr;
    } else {
        outRssi = rssiOffset + static_cast<float>(rawRssi);
    }
}

/**
 * @brief Appends an authenticated packet capture context thread-safely into the doubly-linked memory matrix.
 * @param packet Reference data container holding the payload and precise float telemetry metrics.
 * @param sequence Index mapping tracking positions from boot.
 * @return The runtime node database row identifier key mapped to the historical record.
 */
uint32_t LoraController::addPacketToList(const LoraPacket& packet, uint32_t sequence) {
    uint32_t assignedId = 0;
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        LoraPacketNode node;
        node.databaseId = m_state.nextDatabaseId++;
        node.packetSequence = sequence;
        node.rssi = packet.rssi;
        node.snr = packet.snr;
        node.length = packet.length;
        node.payload.assign(packet.payload, packet.payload + packet.length);

        m_state.packetList.push_back(node);
        assignedId = node.databaseId;
        m_state.totalPacketsReceived = sequence;

        enforceHistoryLimit();
        xSemaphoreGive(m_stateMutex);
    }
    return assignedId;
}

/**
 * @brief Prunes tail allocations once storage maps exceed user-allocated capacity configurations.
 * @note This call assumes the internal mutex guard context lock is already actively held by the thread.
 */
void LoraController::enforceHistoryLimit() {
    while (m_state.packetList.size() > m_state.maxHistorySize) {
        m_state.packetList.pop_front();
    }
}

/**
 * @brief Locates and discards an individual historical packet data node row via primary database key matching.
 * @param databaseId Target tracking key identifier specifying the exact instance container to purge.
 * @return True upon matching row isolation and destruction clearance, false if ID lookup fails.
 */
bool LoraController::removePacketFromList(uint32_t databaseId) {
    bool removed = false;
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        for (auto it = m_state.packetList.begin(); it != m_state.packetList.end(); ++it) {
            if (it->databaseId == databaseId) {
                m_state.packetList.erase(it);
                removed = true;
                break;
            }
        }
        xSemaphoreGive(m_stateMutex);
    }
    return removed;
}

/**
 * @brief Wipes the active history matrix container clean, resetting allocations instantly.
 */
void LoraController::clearPacketList() {
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.packetList.clear();
        xSemaphoreGive(m_stateMutex);
    }
}

/**
 * @brief Re-orders historical database lists prioritizing records via precise RSSI metric thresholds.
 * @param ascending True shifts weaker signboards forward, false stacks optimal power values first.
 */
void LoraController::sortListByRssi(bool ascending) {
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.packetList.sort([ascending](const LoraPacketNode& a, const LoraPacketNode& b) {
            return ascending ? (a.rssi < b.rssi) : (a.rssi > b.rssi);
        });
        xSemaphoreGive(m_stateMutex);
    }
}

/**
 * @brief Enforces native baseline sequence orders back over active historical tracking allocations.
 */
void LoraController::sortListChronologically() {
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.packetList.sort([](const LoraPacketNode& a, const LoraPacketNode& b) {
            return a.databaseId < b.databaseId;
        });
        xSemaphoreGive(m_stateMutex);
    }
}

/**
 * @brief Safely clones active database tracks within temporary storage allocations to protect ongoing engine execution.
 * @return A deep copy duplicate containing isolated copies of the tracked packet records database tracking array.
 */
std::list LoraController::getPacketListCopy() {
    std::list listCopy;
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        listCopy = m_state.packetList;
        xSemaphoreGive(m_stateMutex);
    }
    return listCopy;
}

/**
 * @brief Encapsulates a data memory segment into RF frames and waits blockingly for full hardware transmission.
 * @param buffer Reference pointer targeting outbound binary data memory segments.
 * @param length Byte element count limits to package inside outbound transport blocks.
 * @return ESP_OK upon confirmed transmission completion, alternative codes upon timeouts or dropouts.
 */
esp_err_t LoraController::transmitFrame(const uint8_t* buffer, uint8_t length) {
    if (m_spiHandle == nullptr || buffer == nullptr || length == 0) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Initiating transmission sequence: Sending %u bytes.", length);

    // TODO: Track signal-to-noise regression metrics during transmit states via calculateTelemetry.
    // Consider sampling RegRssiWideband (0x2C) here to evaluate clear channel assessment (CCA).

    // 1. Prepare hardware for TX
    writeRegister(0x01, 0x81U); // STDBY Mode
    writeRegister(0x0D, 0x00U); // RegFifoTxBaseAddr
    writeRegister(0x09, 0x00U); // RegFifoAddrPtr

    // 2. Fill the FIFO buffer
    for (uint8_t i = 0; i < length; i++) {
        writeRegister(0x00, buffer[i]);
    }

    // 3. Set payload length and remap DIO0 to TxDone (0x01 mapped to DIO0)
    writeRegister(0x22, length); // RegPayloadLength
    writeRegister(0x40, 0x40U);  // RegDioMapping1: Map DIO0 onto TxDone

    // Clear any leftover IRQ flags before transmission starts
    writeRegister(0x12, 0xFFU);  // RegIrqFlags: Clear all flags

    // Make sure the binary semaphore is empty before we wait
    xSemaphoreTake(m_isrSemaphore, 0);

    // 4. Trigger actual physical transmission
    writeRegister(0x01, 0x83U);  // RegOpMode: TX Mode Trigger

    // 5. Block and wait for the hardware ISR to signal TxDone (with a 2-second safety timeout)
    const TickType_t txTimeoutTicks = pdMS_TO_TICKS(2000);
    if (xSemaphoreTake(m_isrSemaphore, txTimeoutTicks) == pdTRUE) {
        uint8_t irqFlags = readRegister(0x12);
        writeRegister(0x12, irqFlags); // Clear the flag in hardware

        if ((irqFlags & 0x08U) != 0U) { // 0x08 is the TxDone bit
            ESP_LOGI(TAG, "Transmission completed successfully.");

            if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
                m_state.totalPacketsTransmitted++;
                xSemaphoreGive(m_stateMutex);
            }

            // Re-engage Receiver Mode immediately so we don't miss incoming frames
            setModeReceiver();
            return ESP_OK;
        }
    }

    // If we reach this point, either a timeout occurred or TxDone flag was missing
    ESP_LOGE(TAG, "Transmission failed! TX Timeout or hardware error.");

    // Force radio back to safety and recover listening state
    setModeReceiver();
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Core trampoline context function remapping FreeRTOS pointer entries back into our object instances.
 * @param pvParameters Object execution context tracking reference token.
 */
void LoraController::taskTrampoline(void pvParameters) {
    auto* instance = static_cast<LoraController*>(pvParameters);
    if (instance != nullptr) {
        instance->run();
    }
    vTaskDelete(nullptr);
}

/**
 * @brief Spawns the processing task directly onto an isolated core map allocation framework.
 * @param priority Schedulers assignment hierarchy profiling ranking parameter.
 */
void LoraController::startTask(uint32_t priority) {
    xTaskCreatePinnedToCore(LoraController::taskTrampoline, "LORA_EngineTask", 4096, this, priority, nullptr, 1);
}

/**
• @brief Primary operational processing thread executing real-time ISR evaluation workflows.
*/
void LoraController::run() {
    if (initializeHardware() != ESP_OK) return;
    setModeReceiver();

    extern DisplayController* display;
    static uint32_t packetCounter = 0;

    while (true) {
        if (xSemaphoreTake(m_isrSemaphore, portMAX_DELAY) == pdTRUE) {
            uint32_t irqFlags = readRegister(0x12);
            writeRegister(0x12, static_cast<uint8_t>(irqFlags));

            if ((irqFlags & 0x40U) != 0U) { // RxDone Flag
                packetCounter++;
                LoraPacket packet = {};
                packet.length = readRegister(0x13);

                uint32_t currentFifoAddress = readRegister(0x10);
                writeRegister(0x09, static_cast<uint8_t>(currentFifoAddress));

                for (uint8_t i = 0; i < packet.length; i++) {
                    packet.payload[i] = readRegister(0x00);
                }

                //packet.rssi = static_cast<int8_t>(static_cast<int>(readRegister(0x1A)) - 157);
                //packet.snr  = static_cast<int8_t>(static_cast<int>(readRegister(0x1B)) / 4);

                // Read raw values from transceiver registers
                uint8_t rawRssiReg = readRegister(0x1A);
                uint8_t rawSnrReg  = readRegister(0x1B);

                // Dynamically process telemetry parameters with floating point precision
                calculateTelemetry(rawRssiReg, rawSnrReg, packet.rssi, packet.snr);

                // Insert into the dynamic doubly-linked list state table
                uint32_t nodeDbId = addPacketToList(packet, packetCounter);
                // Formatted terminal trace using fixed precision formatting (.1f)
                printf("\r\n--------------------------------------------------\r\n");
                printf("[LoRa RX] Packet #%lu successfully received!\r\n", (unsigned long)packetCounter);
                printf("  Database ID : %lu\r\n", (unsigned long)nodeDbId);
                printf("  Length      : %u Bytes\r\n", packet.length);
                printf("  RSSI        : %.1f dBm (Signal Strength)\r\n", packet.rssi);
                printf("  SNR         : %.1f dB  (Signal-to-Noise Ratio)\r\n", packet.snr);
                printf("  Content     : \"");
                for (uint8_t i = 0; i < packet.length; i++) {
                    char c = static_cast<char>(packet.payload[i]);
                    if (c >= 32 && c <= 126) printf("%c", c);
                    else printf("~");
                }
                printf("\"\r\n");

                printf("  Hex         : ");
                for (uint8_t i = 0; i < packet.length; i++)
                    printf("%02X ", packet.payload[i]);

                printf("\r\n--------------------------------------------------\r\n");
                printf("%s # ", Config::PROJECT_TITLE);
                fflush(stdout);

                if (display != nullptr) {
                    char displayBuffer[16];
                    snprintf(displayBuffer, sizeof(displayBuffer), "LoRa rx #%03u", (unsigned int)(packetCounter % 1000));
                    display->printMessage(2, displayBuffer);
                }
            }
        }
    }
}
