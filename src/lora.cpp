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
    m_stateMutex   = xSemaphoreCreateMutex();

    if (m_isrSemaphore == nullptr || m_stateMutex == nullptr) {
        ESP_LOGE(TAG, "Fatal: Failed to allocate LoRa synchronization primitives.");
        return;
    }
}

LoraController::~LoraController() {
    if (m_spiHandle != nullptr) {
        spi_bus_remove_device(m_spiHandle);
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

/**
 * @brief Low-level driver method to override an option byte parameter inside a transceiver hardware register.
 * @param address The hardware register memory space tracking coordinate byte.
 * @param value The option layout bits context to be committed directly into the transceiver.
 */
void LoraController::writeRegister(uint8_t address, uint8_t value) {
    if (m_spiHandle == nullptr) return;

    uint8_t txData[2] = { static_cast<uint8_t>(address | 0x80U), value };

    spi_transaction_t trans = {};
    trans.length = 16;
    trans.tx_buffer = txData;

    spi_device_transmit(m_spiHandle, &trans);
}

// Suppress the ISO C++ braced-groups macro expansion warning for the FreeRTOS portYIELD macro
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
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

    // Invoke the modular modem configuration
    applyModemConfig();

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    // Configure interrupt triggering on the rising edge (PosEdge)
    err = gpio_set_intr_type(Config::LORA_IRQ_PIN, GPIO_INTR_POSEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO interrupt type: %s", esp_err_to_name(err));
        return err;
    }
   // Bind the static ISR handler to the physical pin context
    err = gpio_isr_handler_add(Config::LORA_IRQ_PIN, LoraController::gpioIsrHandler, m_isrSemaphore);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GPIO ISR handler: %s", esp_err_to_name(err));
        return err;
    }

    // MARK STATE MATRIX READY (Thread-Safe Status Update)
    // We use a safe timeout here. If the mutex is permanently blocked during boot,
    // we must escalate this as a system fault instead of returning ESP_OK.
    if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        m_state.isInitialized = true;
        m_state.frequencyHz   = Config::LORA_DEFAULT_FREQ_HZ;
        xSemaphoreGive(m_stateMutex);
    } else {
        ESP_LOGE(TAG, "Fatal: State mutex acquisition timed out during hardware registration.");
        return ESP_ERR_TIMEOUT; // Escalate the mutex block as a hard boot failure
    }

    ESP_LOGI(TAG, "SX1276 LoRa transceiver attached successfully via SPI (Version: 0x%02X).", version);
    return ESP_OK;
}

void LoraController::setModeReceiver() {
    // DIO0 auf RxDone mappen (Bit 7-6 auf 00)
    writeRegister(0x40, 0x00U);

    // In den kontinuierlichen Empfangsmodus schalten
    writeRegister(0x01, 0x85U);

    ESP_LOGD(TAG, "Radio state changed to RXCONTINUOUS");
}

/**
 * @brief Applies the modem profile, RF carrier frequency, and network sync word.
 *        Note: The transceiver must transition through SLEEP mode to apply these alterations safely.
 */
void LoraController::applyModemConfig() {
    // Force Sleep + LoRa mode to unlock configuration registers
    writeRegister(0x01, 0x80U);
    vTaskDelay(pdMS_TO_TICKS(10));



    // Apply RF carrier center frequency (e.g., MeshCom 433.175 MHz)
    writeRegister(0x06, Config::LORA_FRF_MSB);
    writeRegister(0x07, Config::LORA_FRF_MID);
    writeRegister(0x08, Config::LORA_FRF_LSB);


    // just write the m_state frequency....
    m_state.frequencyHz = Config::LORA_DEFAULT_FREQ_HZ; // @TODO: remove this line after implementation of the doubly linked list


    // Configure modem bandwidth, spreading factor, and coding rate
    writeRegister(0x1D, Config::LORA_MODEM_CONFIG_1);
    writeRegister(0x1E, Config::LORA_MODEM_CONFIG_2);

    // Set the network sync word (0x34 for open/public amateur networks like MeshCom)
    writeRegister(0x39, Config::LORA_SYNC_WORD);

    // Set the preamble length (0x0C instead of 0x08 for more stability, see docs/lora/device-settings.md)
    writeRegister(0x20, Config::LORA_PREAMBLE_LENGTH_MSB);
    writeRegister(0x21, Config::LORA_PREAMBLE_LENGTH_LSB);

    // Transition to STANDBY to allow the internal PLL to lock and stabilize
    writeRegister(0x01, 0x81U);
    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_LOGI(TAG, "Radio profile applied: 433.175 MHz | SF11 | BW 250kHz | SyncWord 0x34");
}


#ifndef MATH_COMPARE
    #define MATH_COMPARE 0
#endif


void LoraController::setFrequency(uint32_t freq_hz) {
    if (m_spiHandle == nullptr) return;

    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.frequencyHz = freq_hz;

        // 1. Fetch current mode and shift transceiver into Standby (0x01)
        // Changing RF carrier registers is strictly prohibited while actively radiating or sampling
        const uint8_t previousMode = readRegister(0x01); // RegOpMode
        writeRegister(0x01, static_cast<uint8_t>((previousMode & 0xF8U) | 0x01U));
        vTaskDelay(pdMS_TO_TICKS(2));

        // 2. High-speed fractional frequency calculation
        // Formula optimized from: (freq_hz * 2^19) / 32,000,000 b/c 32,000,000 can be seen as 2^11 * 5^6:
        // we see freq_hz * 2^(19-11)/5^6 which is: freq_hz * 2^8 / 15625, i.e.
        // reduced to exact fraction: 256 / 15,625
        // or even more using shift operation, we can switch on a compare of all three methods with #MATH_COMPARE 1

    #if (MATH_COMPARE == 1)
        // =================================================================
        // BENCHMARK MODE (Runs all 3 variants and measures CPU cycles)
        // =================================================================
        uint32_t start_cycles = esp_cpu_get_cycle_count();
        [[maybe_unused]] volatile uint32_t frf_long = static_cast<uint32_t>((static_cast<uint64_t>(freq_hz) << 19) / 32000000ULL);
        uint32_t cycles_long = esp_cpu_get_cycle_count() - start_cycles;

        start_cycles = esp_cpu_get_cycle_count();
        [[maybe_unused]] volatile uint32_t frf_64bit = static_cast<uint32_t>((static_cast<uint64_t>(freq_hz) * 256ULL) / 15625ULL);
        uint32_t cycles_64bit = esp_cpu_get_cycle_count() - start_cycles;

        start_cycles = esp_cpu_get_cycle_count();
        volatile uint32_t frf_shift = static_cast<uint32_t>((static_cast<uint64_t>(freq_hz) * 1125899907ULL) >> 36);
        uint32_t cycles_shift = esp_cpu_get_cycle_count() - start_cycles;

        printf("[MATH_COMPARE] Freq: %lu Hz -> long 64bit: %lu cycles | 64bit: %lu cycles | Shift: %lu cycles\n",
                      freq_hz, cycles_long, cycles_64bit, cycles_shift);

        const uint32_t frf = frf_shift; // Fallback to the safe bit-shift variant
    #else
        // =================================================================
        // PRODUCTION MODE (Highly optimized, zero benchmark overhead)
        // =================================================================
        const uint32_t frf = static_cast<uint32_t>((static_cast<uint64_t>(freq_hz) * 1125899907ULL) >> 36);
    #endif


        // 3. Shift the 24-bit frequency tracking token across the three consecutive hardware registers
        writeRegister(0x06, static_cast<uint8_t>((frf >> 16) & 0xFFU)); // RegFrfMsb
        writeRegister(0x07, static_cast<uint8_t>((frf >> 8) & 0xFFU));  // RegFrfMid
        writeRegister(0x08, static_cast<uint8_t>(frf & 0xFFU));         // RegFrfLsb

        // 4. Restore previous operational transceiver path configuration
        writeRegister(0x01, previousMode);
        xSemaphoreGive(m_stateMutex);

        // 5. Force state synchronization and PLL locking verification
        setModeReceiver();

        // High-priority diagnosis logging bypasses CLI lock buffers uncensored
        printf("\r\n[LoRa HW] Frequency successfully shifted to %lu Hz (FRF Word: 0x%06X)\r\n",
            (unsigned long)freq_hz, (unsigned int)(frf & 0xFFFFFFU));
        fflush(stdout);
    }
}

void LoraController::setBandwidth(uint32_t bw_khz) {
    if (m_spiHandle == nullptr) return;
    uint8_t bw_setting = 0x00U;
    bool valid = false;

    if (bw_khz == 62)   { bw_setting = 0x60U; valid = true; }
    if (bw_khz == 125)  { bw_setting = 0x70U; valid = true; }
    if (bw_khz == 250)  { bw_setting = 0x80U; valid = true; }
    if (bw_khz == 500)  { bw_setting = 0x90U; valid = true; }

    if (!valid) return;

    writeRegister(0x01, 0x81U);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t current_config = readRegister(0x1D);
    current_config = static_cast<uint8_t>(current_config & 0x0FU);
    current_config = static_cast<uint8_t>(current_config | bw_setting);

    writeRegister(0x1D, current_config);
    writeRegister(0x01, 0x85U);
    ESP_LOGI(TAG, "Modem bandwidth dynamically shifted to %lu kHz.", (unsigned long)bw_khz);
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

void LoraController::run() {
    // Lambda Object:
    auto utf8_advance = [](const std::string& str, size_t start_byte, size_t num_chars) -> size_t {
        size_t i = start_byte;
        size_t chars_counted = 0;
        while (i < str.length() && chars_counted < num_chars) {
            if (((unsigned char)str[i] & 0xC0) != 0x80) chars_counted++;
            i++;
        }
        while (i < str.length() && ((unsigned char)str[i] & 0xC0) == 0x80) i++;
        return i;
    };

    if (initializeHardware() != ESP_OK) {
        ESP_LOGE(TAG, "Critical: LoRa init failed. Thread aborted.");
        return;
    }

    setModeReceiver();
    extern DisplayController* display; // Reference the global display singleton
    static uint32_t packetCounter = 0; // overflow in abt. 272 years (1 packet every 2 seconds)

    ESP_LOGI(TAG, "LoRa RX was initialized and is running");

    while (true) {
        // CHANGE: wait a maximum of 500 ms instead of infinity long
        if (xSemaphoreTake(m_isrSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
        // if (xSemaphoreTake(m_isrSemaphore, portMAX_DELAY) == pdTRUE) {
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
                // Calculate the telemetry data
                // packet.rssi = static_cast<int8_t>(static_cast<int>(readRegister(0x1A)) - 157);
                // packet.snr  = static_cast<int8_t>(static_cast<int>(readRegister(0x1B)) / 4);
                // Read raw values from transceiver registers
                uint8_t rawRssiReg = readRegister(0x1A);
                uint8_t rawSnrReg  = readRegister(0x1B);

                // Dynamically process telemetry parameters with floating point precision
                calculateTelemetry(rawRssiReg, rawSnrReg, packet.rssi, packet.snr);

                // Formatted terminal trace using fixed precision formatting (.1f)
                printf("\r\n--------------------------------------------------\r\n");
                printf("[LoRa RX] Packet #%lu successfully received!\r\n", (unsigned long)packetCounter);
                // printf("  Database ID : %lu\r\n", (unsigned long)nodeDbId);
                printf("  Length      : %u Bytes\r\n", packet.length);
                printf("  RSSI        : %.1f dBm (Signal Strength)\r\n", packet.rssi);
                printf("  SNR         : %.1f dB  (Signal-to-Noise Ratio)\r\n", packet.snr);
                printf("  Content: \"");
                for (uint8_t i = 0; i < packet.length; i++) {
                    char c = static_cast<char>(packet.payload[i]);
                    // Only print printable characters to avoid confusing the terminal
                    if (c >= 32 && c <= 126)
                        printf("%c", c);
                    else
                        printf("~");   // not printable chars
                }
                printf("\"\r\n  Hex    : ");
                for (uint8_t i = 0; i < packet.length; i++)
                    printf("%02X ", packet.payload[i]);

                printf("\r\n--------------------------------------------------\r\n");
                printf("%s # ", Config::PROJECT_TITLE); // CLI prompt
                fflush(stdout);

                // FIXED: Uses the new parameter order (line first, text second)
                if (display != nullptr) {
                    char displayBuffer[85]; // worst case 21 chars with each 4 bytes + Nullbyte.

                    snprintf(displayBuffer, sizeof(displayBuffer), "#%03u %4.1fdBm %2.1fdB",
                        (unsigned int)(packetCounter % 1000),
                        packet.rssi,
                        packet.snr);
                    display->printMessage(2, displayBuffer);
                    // we don't care about UTF-8 for now
                    std::string text(reinterpret_cast<const char*>(packet.payload), static_cast<size_t>(packet.length));

                    size_t byte_pos = 0;
                    uint8_t linenr = 3; // start at line 3

                    while ( byte_pos < text.length() && linenr < 8) {
                        // The '::' ensures that the global/local function is called.
                        size_t next_byte_pos = utf8_advance(text, byte_pos, 21);
                        std::string line = text.substr(byte_pos, next_byte_pos - byte_pos);

                        std::strncpy(displayBuffer, line.c_str(), sizeof(displayBuffer ) - 1);
                        displayBuffer[sizeof(displayBuffer) - 1] ='\0';

                        display->printMessage(linenr++, displayBuffer);
                        byte_pos = next_byte_pos;
                    }
                }
            }

            // New: after work or after 500ms delay, the chip has usually changed its mode, we update the state for the m_state:
            uint8_t currentHardwareMode = readRegister(0x01);
            if (xSemaphoreTake(m_stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                m_state.currentMode = currentHardwareMode;
                xSemaphoreGive(m_stateMutex);
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

/**
 * @brief Generates an atomic, high-performance runtime snapshot of configuration parameters and metrics.
 */
LoraStateSnapshot LoraController::getSnapshot() {
    LoraStateSnapshot snapshot = {};

    // Sichere die thread-übergreifende Konsistenz mittels Mutex ab
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {

        // Kopiere die primitiven Werte aus dem internen Zustandstabellen-Struct (m_state)
        snapshot.frequencyHz             = m_state.frequencyHz;
        snapshot.bandwidthKhz            = m_state.bandwidthKhz;
        snapshot.totalPacketsReceived    = m_state.totalPacketsReceived;
        snapshot.totalPacketsTransmitted = m_state.totalPacketsTransmitted;
        snapshot.spreadingFactor         = m_state.spreadingFactor;
        snapshot.codingRate              = m_state.codingRate;
        snapshot.isInitialized           = m_state.isInitialized;

        // Zähle die Elemente in der Liste, ohne die schweren Payload-Vektoren zu kopieren
        // TODO install the package list:
        // snapshot.currentHistoryCount     = m_state.packetList.size();

        // Read Register 0x01 ONLY ONCE to avoid SPI race conditions
        // uint8_t rohofmode = readRegister(0x01);
        // uint8_t rawMode = rohofmode & 0x07U;
        // use the cached mode from RAM instead live register read
        uint8_t rawMode = m_state.currentMode & 0x07U;

        // Print the real, unmanipulated hex value safely
        //printf("Readregister actual raw hex: 0x%02X\r\n", rohofmode);
        //fflush(stdout);

        static const char* const MODE_STRINGS[] = {
            "SLEEP",        // 0x00
            "STANDBY",      // 0x01
            "FSTX",         // 0x02
            "TX",           // 0x03
            "FSRX",         // 0x04
            "RXCONTINUOUS", // 0x05
            "RXSINGLE",     // 0x06
            "CAD"           // 0x07
        };

        strncpy(snapshot.currentModeStr, MODE_STRINGS[rawMode], sizeof(snapshot.currentModeStr) - 1);
        snapshot.currentModeStr[sizeof(snapshot.currentModeStr) - 1] = '\0'; // guarantee NULL termination

        // Release mutex immediately
        xSemaphoreGive(m_stateMutex);
    }

    return snapshot;
}
