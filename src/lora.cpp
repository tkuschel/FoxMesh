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
#include "driver/spi_master.h"

const char* LoraController::TAG = "LORA";

/**
 * @brief Constructor initializing core FreeRTOS synchronization primitives.
 */
LoraController::LoraController() {
    m_isrSemaphore = xSemaphoreCreateBinary();
    m_stateMutex = xSemaphoreCreateMutex();

    if (m_isrSemaphore == nullptr || m_stateMutex == nullptr) {
        ESP_LOGE(TAG, "Fatal: Failed to allocate LoRa synchronization primitives.");
        return;
    }

    esp_err_t err = initializeHardware();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fatal: Hardware initialization failed in constructor!");
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
    if (m_spiHandle == nullptr) return 0;

    // SX1276 Lesemodus: Bit 7 der Adresse muss 0 sein
    uint8_t tx_data[2] = { static_cast<uint8_t>(address & 0x7FU), 0x00U };
    uint8_t rx_data[2] = { 0 };

    spi_transaction_t trans = {};
    trans.length    = 16;       // 16 Bits = 2 Bytes (Adresse + Daten)
    trans.tx_buffer = tx_data;
    trans.rx_buffer = rx_data;

    // Polling-Übertragung für minimale Latenz bei kurzen Register-Zugriffen
    esp_err_t ret = spi_device_polling_transmit(static_cast<spi_device_handle_t>(m_spiHandle), &trans);
    if (ret != ESP_OK) {
        return 0;
    }

    // Das zweite empfangene Byte enthält den Registerwert
    return rx_data[1];
}

/**
 * @brief Low-level driver method to override an option byte parameter inside a transceiver hardware register.
 * @param address The hardware register memory space tracking coordinate byte.
 * @param value The option layout bits context to be committed directly into the transceiver.
 */
void LoraController::writeRegister(uint8_t address, uint8_t value) {
    if (m_spiHandle == nullptr) return;

    // SX1276 Schreibmodus: Bit 7 der Adresse muss 1 sein
    uint8_t tx_data[2] = { static_cast<uint8_t>(address | 0x80U), value };

    spi_transaction_t trans = {};
    trans.length    = 16;       // 16 Bits = 2 Bytes (Adresse + Daten)
    trans.tx_buffer = tx_data;
    trans.rx_buffer = nullptr;  // Wir erwarten keine Rückgabedaten beim Schreiben

    spi_device_polling_transmit(static_cast<spi_device_handle_t>(m_spiHandle), &trans);

    // Live-Diagnose auf der CLI ausgeben (umgeht die 30-Sekunden-Sperre der CLI)
    printf("[SPI TX] Reg 0x%02X <- Value 0x%02X\r\n", address, value);
    fflush(stdout);
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
    esp_err_t ret;

    // 1. Configure reset pin and execute transceiver hardware reset
    gpio_config_t rst_cfg = {};
    rst_cfg.pin_bit_mask = (1ULL << Config::LORA_RST_PIN);
    rst_cfg.mode         = GPIO_MODE_OUTPUT;
    rst_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    rst_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    rst_cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&rst_cfg);

    // Manual hardware reset sequence according to Semtech datasheet
    gpio_set_level(Config::LORA_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(2)); // Hold Low for at least 100 µs
    gpio_set_level(Config::LORA_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait 10 ms until internal oscillator stabilizes

    // 2. Initialize SPI bus configuration
    spi_bus_config_t bus_config = {};
    bus_config.miso_io_num   = Config::LORA_MISO_PIN;
    bus_config.mosi_io_num   = Config::LORA_MOSI_PIN;
    bus_config.sclk_io_num   = Config::LORA_SCK_PIN;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = 0;

    // Utilize SPI2_HOST (HSPI) for device communication
    ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. SX1276 device-specific SPI interface configuration (Chip Select)
    spi_device_interface_config_t dev_config = {};
    dev_config.clock_speed_hz = 8000000;           // 8 MHz SPI clock speed
    dev_config.mode           = 0;                 // SPI Mode 0
    dev_config.spics_io_num   = Config::LORA_CS_PIN;
    dev_config.queue_size     = 7;

    spi_device_handle_t spi_device;
    ret = spi_bus_add_device(SPI2_HOST, &dev_config, &spi_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device hook: %s", esp_err_to_name(ret));
        return ret;
    }

    // Assign handle to the class instance storage
    m_spiHandle = static_cast<void*>(spi_device);

    // 4. Silicon Check: Read back the hardware version register (0x42)
    uint8_t version = readRegister(0x42);
    if (version != 0x12) {
        ESP_LOGE(TAG, "Fatal: SX1276 Silicon check failed! Expected RegVersion 0x12, got 0x%02X", version);
        spi_bus_remove_device(spi_device);
        m_spiHandle = nullptr;
        return ESP_ERR_INVALID_VERSION;
    }

    // 5. Configure hardware interrupt pins (DIO0 & DIO1) for incoming radio events
    // Configure DIO0 (GPIO 26)
    gpio_config_t irq_cfg = {};
    irq_cfg.pin_bit_mask = (1ULL << Config::LORA_IRQ_PIN);
    irq_cfg.mode         = GPIO_MODE_INPUT;
    irq_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    irq_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;  // DIO0 drives actively High on event trigger
    irq_cfg.intr_type    = GPIO_INTR_POSEDGE;     // Trigger on rising edge
    gpio_config(&irq_cfg);

    // Configure DIO1 (GPIO 33)
    gpio_config_t dio1_cfg = {};
    dio1_cfg.pin_bit_mask = (1ULL << Config::LORA_DIO1_PIN);
    dio1_cfg.mode         = GPIO_MODE_INPUT;
    dio1_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    dio1_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;  // DIO1 drives actively High on PLL lock or Rx timeout
    dio1_cfg.intr_type    = GPIO_INTR_POSEDGE;     // Trigger on rising edge
    gpio_config(&dio1_cfg);

    // Install global ISR service (if not already managed elsewhere in the runtime framework)
    gpio_install_isr_service(0);

    // Bind static gpioIsrHandler routines to respective hardware pins, passing the shared binary semaphore context
    gpio_isr_handler_add(Config::LORA_IRQ_PIN, LoraController::gpioIsrHandler, m_isrSemaphore);
    gpio_isr_handler_add(Config::LORA_DIO1_PIN, LoraController::gpioIsrHandler, m_isrSemaphore);

    // 6. Flash initial base modem parameters and frequency values from config attributes
    //writeRegister(0x01, 0x81U); // Shift into Standby mode to grant execution access to configuration spaces
    writeRegister(0x01, 0x89U); // with Low-Frq. 433 MHz
    vTaskDelay(pdMS_TO_TICKS(5));

    writeRegister(0x06, Config::LORA_FRF_MSB);
    writeRegister(0x07, Config::LORA_FRF_MID);
    writeRegister(0x08, Config::LORA_FRF_LSB);
    writeRegister(0x1D, Config::LORA_MODEM_CONFIG_1);
    writeRegister(0x1E, Config::LORA_MODEM_CONFIG_2);

    // --- APPLY SELECTED MESHCOM / NETWORK SYNC PROFILE ---
    writeRegister(0x39, Config::LORA_SYNC_WORD); // Write the dynamic sync word byte

    // Expand hardware preamble length to 12 symbols for robust network correlation
    writeRegister(0x20, 0x00U); // RegPreambleMsb - default
    writeRegister(0x21, Config::LORA_PREAMBLE_LENGTH_LSB); // RegPreambleLsb (12 = 0x0C) - Default start is 0x08

    // --- UNMASK INTERRUPTS ON THE SEMTECH TRANSCEIVER ---
    // Clear all bits in RegIrqFlagsMask (0x11) to enable all hardware interrupt events.
    // Note: A set bit (1) in this register would mask (disable) the corresponding interrupt.
    writeRegister(0x11, 0x00U);

    // Clear any stale interrupt flags to flush the physical DIO0 line back to logic low
    writeRegister(0x12, 0xFFU);

    // Mark driver state as operationally ready thread-safely
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.isInitialized           = true;

        // Single Source of Truth: we use the manreadable values from config
        m_state.frequencyHz             = Config::LORA_DEFAULT_FREQ_HZ;
        m_state.bandwidthKhz            = Config::LORA_DEFAULT_BW_KHZ;
        m_state.spreadingFactor         = Config::LORA_DEFAULT_SF;
        m_state.codingRate              = Config::LORA_DEFAULT_CR;
        xSemaphoreGive(m_stateMutex);
    }

    ESP_LOGI(TAG, "SX1276 LoRa transceiver attached successfully via SPI (Version: 0x%02X).", version);
    return ESP_OK;
}

/**
 * @brief Drives internal radio operational states back into endless listening configurations.
 */
void LoraController::setModeReceiver() {
    if (m_spiHandle == nullptr) return;

    // 1. Force the chip into Standby mode first to safely change operating configurations
    // We preserve Bit 7 (LongRangeMode) to ensure we stay/go into LoRa mode safely
    //writeRegister(0x01, 0x81U);
    writeRegister(0x01, 0x89U); // LowFrequencyModeOn for 433 MHz
    vTaskDelay(pdMS_TO_TICKS(2)); // Allow the chip's internal state machine to stabilize

    // 2. Remap DIO peripheral routes inside the SX1276 (RegDioMapping1 = 0x40)
    // Mask out existing DIO0 (bits 7-6) and DIO1 (bits 5-4) mappings
    // Map DIO0 to RxDone (0x00) and DIO1 to RxTimeout (0x00) within the LoRa mode profile
    uint8_t dioMapping = readRegister(0x40);
    dioMapping &= 0x0FU;
    writeRegister(0x40, dioMapping | 0x00U);

    // -----------------------------------------------------------------
    // NEU: HF-Antennenschalter-Automatik über DIO2/DIO3 aktivieren (RegDioMapping2 = 0x41)
    // Bit 0 im Register 0x41 steuert das automatische Preamble-Detect/Map-Verhalten,
    // welches auf dem TTGO T3 benötigt wird, um den analogen RX-Pfad physisch freizuschalten.
    uint8_t dioMapping2 = readRegister(0x41);
    writeRegister(0x41, dioMapping2 | 0x01U); // Setzt Bit 0 (Map PreambleDetect)
    // -----------------------------------------------------------------

    // 3. Clear any stale interrupt flags before activating the receiver
    // This is vital; otherwise, an un-cleared old flag can instantly lock up the state machine
    writeRegister(0x12, 0xFFU);

    // 2. Commit the SX1276 transceiver directly into continuous listening mode (RXCONTINUOUS = 0x05)
    // Bit 7 = 1 (LoRa Mode), Bit 3 = 1 (LowFrequencyModeOn for 433MHz), Bit 2-0 = 101 (RXCONTINUOUS)
    writeRegister(0x01, 0x8DU);
    vTaskDelay(pdMS_TO_TICKS(1)); // Micro-delay to let the PLL lock sequence initiate smoothly

    // Instant runtime console trace bypassing typical terminal logs throttling locks
    printf("[LoRa] Radio shifted into continuous listening mode (RX).\r\n");
    fflush(stdout);
}

/**
 * @brief Modifies operational carrier targets and maps execution metrics thread-safely.
 * @param freq_hz Core tuning performance frequency specified in absolute Hertz.
 */
void LoraController::setFrequency(uint32_t freq_hz) {
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.frequencyHz = freq_hz;

        // Force trx into Standby mode to update hardware frequency registers safely
        uint8_t previousMode = readRegister(0x01); // RegOpMode
        writeRegister(0x01, (previousMode & 0xF8U) | 0x01U); // Set to Standby (0x01)
        vTaskDelay(pdMS_TO_TICKS(5));

        // Calculate the 24-bit frequency word using 64-bit precision to avoid intermediate overflow
        // Formula: FRF = (freq_hz * 2^19) / 32 000 000
        uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000ULL;

        // Split the 24-bit tracking token across the three consecutive hardware registers
        writeRegister(0x06, static_cast<uint8_t>((frf >> 16) & 0xFFU)); // RegFrfMsb
        writeRegister(0x07, static_cast<uint8_t>((frf >> 8) & 0xFFU));  // RegFrfMid
        writeRegister(0x08, static_cast<uint8_t>(frf & 0xFFU));         // RegFrfLsb

        ///// writeRegister(0x01, previousMode);

        xSemaphoreGive(m_stateMutex);

        setModeReceiver();

        // Geändertes printf umgeht die CLI-Logsperre unzensiert
        printf("\r\n[LoRa HW] Frequency successfully shifted to %lu Hz (0x%06X)\r\n",
               (unsigned long)freq_hz, (unsigned int)frf);
        fflush(stdout);
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
    if (m_spiHandle == nullptr) return;

    // Map die gängigen kHz-Werte auf die Bit-Konfigurationen des SX1276 (RegModemConfig1)
    uint8_t bw_val = 0;
    if (bw_khz == 62)        bw_val = 0x06; // 62.5 kHz
    else if (bw_khz == 125)  bw_val = 0x07; // 125 kHz
    else if (bw_khz == 250)  bw_val = 0x08; // 250 kHz
    else if (bw_khz == 500)  bw_val = 0x09; // 500 kHz
    else {
        printf("Error: Unsupported bandwidth %lu kHz\r\n", (unsigned long)bw_khz);
        fflush(stdout);
        return;
    }

    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.bandwidthKhz = bw_khz;

        // 1. Sichere den aktuellen Modus des Radios
        uint8_t previousMode = readRegister(0x01); // RegOpMode

        // 2. Schalte das Radio kurz in den Standby-Modus (Modem-Registeränderung nur im Standby/Sleep erlaubt!)
        writeRegister(0x01, (previousMode & 0xF8U) | 0x01U);
        vTaskDelay(pdMS_TO_TICKS(2));

        // 3. Lies RegModemConfig1 (0x1D), maskiere die alten Bandbreiten-Bits (7-4) aus und setze die neuen
        uint8_t config1 = readRegister(0x1D);
        config1 = (config1 & 0x0FU) | static_cast<uint8_t>(bw_val << 4);
        writeRegister(0x1D, config1);

        // 4. Stelle den ursprünglichen Radio-Modus wieder her (z.B. zurück in den RX-Modus)
        writeRegister(0x01, previousMode);

        xSemaphoreGive(m_stateMutex);

        // Klares, ungefiltertes CLI-Feedback (umgeht die 30-Sekunden-Logsperre)
        printf("\r\n[LoRa HW] Bandwidth successfully updated to %lu kHz (Reg0x1D: 0x%02X)\r\n",
               (unsigned long)bw_khz, config1);
        fflush(stdout);
    }
}

/**
 * @brief Overrides the operational hardware spreading factor configuration.
 * @param sf Target spreading factor (typically 6 to 12).
 */
void LoraController::setSpreadingFactor(uint8_t sf) {
    if (m_spiHandle == nullptr) return;
    if (sf < 6 || sf > 12) {
        printf("Error: Unsupported spreading factor SF%u (Allowed: 6-12)\r\n", sf);
        fflush(stdout);
        return;
    }

    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.spreadingFactor = sf;

        uint8_t previousMode = readRegister(0x01); // RegOpMode
        writeRegister(0x01, (previousMode & 0xF8U) | 0x01U); // Standby
        vTaskDelay(pdMS_TO_TICKS(2));

        // SF liegt in den oberen 4 Bits (7-4) von RegModemConfig2 (0x1E)
        uint8_t config2 = readRegister(0x1E);
        config2 = (config2 & 0x0FU) | static_cast<uint8_t>(sf << 4);
        writeRegister(0x1E, config2);

        // Sonderfall für SF6 (erfordert zusätzliches Handling im Register 0x31/0x37 laut Datenblatt)
        if (sf == 6) {
            uint8_t detectOptimize = readRegister(0x31);
            writeRegister(0x31, (detectOptimize & 0xF8U) | 0x05U);
            writeRegister(0x37, 0x0CU);
        } else {
            uint8_t detectOptimize = readRegister(0x31);
            writeRegister(0x31, (detectOptimize & 0xF8U) | 0x03U);
            writeRegister(0x37, 0x0AU);
        }

        writeRegister(0x01, previousMode); // Zurück in den Vorzustand
        xSemaphoreGive(m_stateMutex);

        printf("\r\n[LoRa HW] Spreading Factor updated to SF%u (Reg0x1E: 0x%02X)\r\n", sf, config2);
        fflush(stdout);
    }
}

/**
 * @brief Overrides the operational hardware coding rate configuration.
 * @param cr_denominator Target denominator (e.g. 5 for 4/5, 6 for 4/6, etc.).
 */
void LoraController::setCodingRate(uint8_t cr_denominator) {
    if (m_spiHandle == nullptr) return;
    if (cr_denominator < 5 || cr_denominator > 8) {
        printf("Error: Unsupported coding rate 4/%u (Allowed: 4/5 - 4/8)\r\n", cr_denominator);
        fflush(stdout);
        return;
    }

    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        m_state.codingRate = cr_denominator;

        uint8_t previousMode = readRegister(0x01); // RegOpMode
        writeRegister(0x01, (previousMode & 0xF8U) | 0x01U); // Standby
        vTaskDelay(pdMS_TO_TICKS(2));

        // CR-Wert für das Register berechnen (CR = cr_denominator - 4)
        uint8_t cr_register_val = cr_denominator - 4;

        // Coding Rate liegt in den Bits 3-1 von RegModemConfig1 (0x1D)
        uint8_t config1 = readRegister(0x1D);
        config1 = (config1 & 0xF1U) | static_cast<uint8_t>(cr_register_val << 1);
        writeRegister(0x1D, config1);

        writeRegister(0x01, previousMode);
        xSemaphoreGive(m_stateMutex);

        printf("\r\n[LoRa HW] Coding Rate updated to 4/%u (Reg0x1D: 0x%02X)\r\n", cr_denominator, config1);
        fflush(stdout);
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
std::list<LoraPacketNode> LoraController::getPacketListCopy() {
    std::list<LoraPacketNode> listCopy;
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
    //writeRegister(0x01, 0x81U); // STDBY Mode --> changed to:
    writeRegister(0x01, 0x89U);
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

    // 4. Trigger actual physical transmission with Low Frequency Mode enabled (0x08)
    // Bit 7 = 1 (LoRa Mode)
    // Bit 3 = 1 (LowFrequencyModeOn for 433MHz)
    // Bits 2-0 = 011 (TX Mode)
    // Formula: 0x80 | 0x08 | 0x03 = 0x8BU
    writeRegister(0x01, 0x8BU);  // RegOpMode: TX Mode Trigger (433MHz Band)

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
void LoraController::taskTrampoline(void* pvParameters) {
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
    //if (initializeHardware() != ESP_OK) return;  // moved to the constructor

    // Check if hardware is ready
    bool init_success = false;
    if (xSemaphoreTake(m_stateMutex, portMAX_DELAY) == pdTRUE) {
        init_success = m_state.isInitialized;
        xSemaphoreGive(m_stateMutex);
    }

    if (!init_success) {
        ESP_LOGE(TAG, "Engine abort: Hardware not initialized.");
        vTaskDelete(nullptr);
        return;
    }

    setModeReceiver();

    extern DisplayController* display;
    static uint32_t packetCounter = 0;

    while (true) {
        // waits till GPIO interrupt (DIO0) leases the semaphore
        if (xSemaphoreTake(m_isrSemaphore, portMAX_DELAY) == pdTRUE) {

            // 1. Read the active interrupt flags and immediatly clear them (writing 1)
            uint32_t irqFlags = readRegister(0x12);
            writeRegister(0x12, static_cast<uint8_t>(irqFlags));

            // 2. Verify if the RxDone flag (bit 6) is set
            if ((irqFlags & 0x40U) != 0U) { // RxDone Flag
                packetCounter++;
                LoraPacket packet = {};
                packet.length = readRegister(0x13);

                // 3. Reset the internal FIFO read pointe to the start address

                uint32_t currentFifoAddress = readRegister(0x10);
                writeRegister(0x09, static_cast<uint8_t>(currentFifoAddress));

                for (uint8_t i = 0; i < packet.length; i++) {
                    packet.payload[i] = readRegister(0x00); // RegFifo
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
            // 5. Explicitly re-engage continous receiver mode
            setModeReceiver();
        }
    }
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
        snapshot.currentHistoryCount     = m_state.packetList.size();

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
        uint8_t rawMode = readRegister(0x01) & 0x07U;
        strncpy(snapshot.currentModeStr, MODE_STRINGS[rawMode], sizeof(snapshot.currentModeStr));

        // Mutex sofort wieder freigeben, um das LoRa-System nicht zu blockieren
        xSemaphoreGive(m_stateMutex);
    }

    return snapshot;
}
