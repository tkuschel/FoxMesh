/**
 * @file lora_types.hpp
 * @brief Definition of data structures, nodes, and state tables for the LoRa driver.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <list>
#include <vector>
#pragma GCC diagnostic pop
#include <stdint.h>

/**
 * @brief Structure representing a single packet node within the doubly-linked tracking list.
 */
struct LoraPacketNode {
    /** @brief Dynamic raw binary storage buffer containing the received message payload. */
    std::vector<uint8_t> payload;

    /** @brief Unique auto-incremented database identifier for targeting explicit items. */
    uint32_t databaseId;

    /** @brief The absolute sequence index counting the packets received since device boot. */
    uint32_t packetSequence;

    /** @brief Received Signal Strength Indicator (RSSI) measured in dBm. */
    float rssi;

    /** @brief Signal-to-Noise Ratio (SNR) measured in dB. */
    float snr;

    /** @brief Payload size explicitly specified in bytes. */
    uint8_t length;
};

/**
 * @brief Runtime operational state table monitoring configurations and historical packet data.
 * @note Optimized using "largest-to-smallest" data type ordering to completely eliminate memory padding.
 */
struct LoraStateTable {
    /** @brief Central database component built on a doubly-linked list for versatile node operations. (Contains pointers) */
    std::list<LoraPacketNode> packetList;

    /** @brief Soft-limit threshold capping the maximum number of history records stored before auto-purge. (4 or 8 Bytes) */
    size_t maxHistorySize = 10;

    /** @brief RF carrier operating frequency configured in Hertz. (4 Bytes) */
    uint32_t frequencyHz = 433175000;

    /** @brief Accumulator metric track-counting successful ingress frames. (4 Bytes) */
    uint32_t totalPacketsReceived = 0;

    /** @brief Accumulator metric track-counting egress frames transmitted. (4 Bytes) */
    uint32_t totalPacketsTransmitted = 0;

    /** @brief Identity sequencer pointer controlling the next node's databaseId field. (4 Bytes) */
    uint32_t nextDatabaseId = 1;

    /** @brief Modem bandwidth performance target configured in kHz. (4 Bytes) */
    uint32_t bandwidthKhz = 250;

    /** @brief LoRa Spreading Factor configuration (typically ranges between 6 and 12). (1 Byte) */
    uint8_t spreadingFactor = 11;

    /** @brief Coding Rate denominator setting (e.g., value 6 yields a 4/6 coding protection overhead). (1 Byte) */
    uint8_t codingRate = 6;

    /** @brief True if hardware was successfully validated and attached during initialization. (1 Byte) */
    bool isInitialized = false;

    // The three 1-byte variables at the bottom align perfectly into a 32-bit slot, leaving minimal trailing padding.
};

/**
 * @brief Thread-isolated telemetry snapshot for UI displays and remote diagnostics.
 * @note Optimized using "largest-to-smallest" data type ordering to minimize memory padding.
 */
struct LoraStateSnapshot {
    /** @brief Soft-limit threshold capping the maximum number of history records stored before auto-purge. (4 or 8 Bytes) */
    size_t currentHistoryCount;

    /** @brief RF carrier operating frequency configured in Hertz. (4 Bytes) */
    uint32_t frequencyHz;

    /** @brief Modem bandwidth performance target configured in kHz. (4 Bytes) */
    uint32_t bandwidthKhz;

    /** @brief Accumulator metric track-counting successful ingress frames. (4 Bytes) */
    uint32_t totalPacketsReceived;

    /** @brief Accumulator metric track-counting egress frames transmitted. (4 Bytes) */
    uint32_t totalPacketsTransmitted;

    /** @brief LoRa Spreading Factor configuration (typically ranges between 6 and 12). (1 Byte) */
    uint8_t spreadingFactor;

    /** @brief Coding Rate denominator setting (e.g., value 6 yields a 4/6 coding protection overhead). (1 Byte) */
    uint8_t codingRate;

    /** @brief True if hardware was successfully validated and attached during initialization. (1 Byte) */
    bool isInitialized;

    /** @brief Current Mode  */
    char currentModeStr[16];
    // The three 1-byte variables at the bottom align perfectly into a 32-bit slot, leaving only 1 byte of trailing padding.
};
