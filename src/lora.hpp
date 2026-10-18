/**
 * @file lora.hpp
 * @brief Thread-safe asynchronous Semtech SX1276 LoRa transceiver driver interface.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lora_types.hpp"
#include "esp_err.h"

/**
 * @brief Simple internal abstraction layer mirroring the raw incoming packet payload.
 */
struct LoraPacket {
    float rssi;
    float snr;
    uint8_t length;
    uint8_t payload;
};

/**
 * @brief Controller class orchestrating hardware configurations, data transfers, and metrics collection.
 */
class LoraController {
private:
    static const char* TAG;                     ///< Logging identifier tag for ESP_LOG statements.
    void* m_spiHandle = nullptr;                ///< SPI device interface reference token pointer.
    SemaphoreHandle_t m_isrSemaphore = nullptr; ///< Binary semaphore facilitating hardware-to-task sync notifications.

    LoraStateTable m_state;                     ///< Internal volatile tracking record data context.
    SemaphoreHandle_t m_stateMutex = nullptr;   ///< Mutex primitive enforcing mutual exclusion over m_state modifications.

    /**
     * @brief Reads a value from an individual internal register address of the transceiver.
     * @param address Target transceiver memory sub-space location address byte.
     * @return The read byte value configuration currently active.
     */
    uint8_t readRegister(uint8_t address);

    /**
     * @brief Writes a specific option parameter directly into a single transceiver register address.
     * @param address Target transceiver memory sub-space location address byte.
     * @param value Setting parameter layout specification code byte.
     */
    void writeRegister(uint8_t address, uint8_t value);

    /**
     * @brief Evaluates raw register readings into calibrated RSSI and SNR telemetry metrics.
     * @details Automatically compensates the RSSI offset base based on the active carrier frequency
     *          band (Low-Band vs High-Band boundary at 525 MHz) as well as sub-noise SNR conditions.
     * @param rawRssi The raw digital output read from RegRssiValue (0x1A).
     * @param rawSnr The raw digital twos-complement output read from RegPktSnrValue (0x1B).
     * @param[out] outRssi Reference destination pointer receiving the calibrated RSSI value in dBm.
     * @param[out] outSnr Reference destination pointer receiving the calibrated SNR value in dB.
     */
    void calculateTelemetry(uint8_t rawRssi, uint8_t rawSnr, int8_t& outRssi, int8_t& outSnr);

    /**
     * @brief Interrupt Service Routine wrapper triggered directly from GPIO state changes.
     * @param arg Opaque payload pointer referencing the target thread synchronization primitive.
     */
    static void IRAM_ATTR gpioIsrHandler(void* arg);

    /**
     * @brief Helper context processing background calculations to prune list overflows.
     * @note This internal call assumes that the m_stateMutex lock has already been successfully acquired.
     */
    void enforceHistoryLimit();

    /**
     * @brief Main processing task thread loop loopback entry execution hub.
     */
    void run();

    /**
     * @brief Static FreeRTOS bridge method mapping execution control context flows.
     * @param pvParameters Context reference address pointing back into the instantiating class layer.
     */
    static void taskTrampoline(void* pvParameters);

public:
    /**
     * @brief Construct a new Lora Controller instance and allocate threading primitives.
     */
    LoraController();

    /**
     * @brief Destroy the Lora Controller object, releasing active allocations and hardware hooks.
     */
    ~LoraController();

    /**
     * @brief Initializes active interface bindings, reset signals, and evaluates valid silicon signatures.
     * @return ESP_OK on operational handshake match, alternative error codes upon physical failures.
     */
    esp_err_t initializeHardware();

    /**
     * @brief Programs basic standard profiling configurations and flags continuous reception monitoring modes.
     */
    void setModeReceiver();

    /**
     * @brief Directly transmits a raw binary frame layout out onto the RF medium link.
     * @param buffer Reference data memory segment holding the outbound bytes block.
     * @param length The complete amount of data characters structured within the payload.
     * @return ESP_OK upon transmission launch clearance, error conditions otherwise.
     */
    esp_err_t transmitFrame(const uint8_t* buffer, uint8_t length);

    /**
     * @brief Spawns a dedicated processing task context onto a specifically designated execution core.
     * @param priority FreeRTOS scheduling execution assignment depth factor.
     */
    void startTask(uint32_t priority);

    /**
     * @brief Thread-safely updates the operational state tracking variables and overrides transceiver registers.
     * @param freq_hz Core RF targeting frequency structured in absolute Hertz scale units.
     */
    void setFrequency(uint32_t freq_hz);

    /**
     * @brief Thread-safely reads out the active target operation frequency metrics context.
     * @return Current system baseline operational tuning value expressed in Hertz units.
     */
    uint32_t getFrequency();

    /**
     * @brief Thread-safely updates the operational modem modulation parameter targets.
     * @param bw_khz Operating modulation channel grid width metric parameters.
     */
    void setBandwidth(uint32_t bw_khz);

    /**
     * @brief Thread-safely logs an incoming transaction capture directly into the historical context nodes collection.
     * @param packet Structural dataset mapping telemetry markers alongside the payload block.
     * @param sequence Absolute hardware sequence reference step index.
     * @return The unique database ID assigned to the tracking instance container wrapper node.
     */
    uint32_t addPacketToList(const LoraPacket& packet, uint32_t sequence);

    /**
     * @brief Thread-safely extracts and purges an explicit single tracking record node entry via database criteria.
     * @param databaseId Unique identifier associated with the specific targeted row block element to discard.
     * @return True if a matching criteria target was located and eliminated, false if no ID matched.
     */
    bool removePacketFromList(uint32_t databaseId);

    /**
     * @brief Thread-safely resets the tracked tracking records data tables list back to empty states.
     */
    void clearPacketList();

    /**
     * @brief Thread-safely sorts the data list according to RSSI signal strength levels.
     * @param ascending If true, lowest signal strengths come first; if false, strongest signals sort to front.
     */
    void sortListByRssi(bool ascending = false);

    /**
     * @brief Thread-safely recovers structural chronological data ordering schemes according to database IDs.
     */
    void sortListChronologically();

    /**
     * @brief Performs a deep copy operation over the linked list under mutex locks to return a safe detached copy.
     * @return A standalone, thread-isolated linked list duplicate snapshot copy containing current history rows.
     */
    std::list<LoraPacketNode> getPacketListCopy();
};
