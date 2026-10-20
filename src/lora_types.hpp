/**
 * @file lora_types.hpp
 * @brief Definition of data structures, nodes, and state tables for the LoRa driver.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

/**
 * @brief Runtime operational state table monitoring configurations and historical packet data.
 * @note Optimized using "largest-to-smallest" data type ordering to completely eliminate memory padding.
 */
struct LoraStateTable {
//    std::list<LoraPacketNode> packetList;    ///< Central database component built on a doubly-linked list for versatile node operations. (Contains pointers)
    size_t maxHistorySize = 10;              ///< Soft-limit threshold capping the maximum number of history records stored before auto-purge. (4 or 8 Bytes)
    uint32_t frequencyHz = 433175000;       ///< RF carrier operating frequency configured in Hertz. (4 Bytes)
    uint32_t totalPacketsReceived = 0;       ///< Accumulator metric track-counting successful ingress frames. (4 Bytes)
    uint32_t totalPacketsTransmitted = 0;    ///< Accumulator metric track-counting egress frames transmitted. (4 Bytes)
    uint32_t nextDatabaseId = 1;             ///< Identity sequencer pointer controlling the next node's databaseId field. (4 Bytes)
    uint32_t bandwidthKhz = 250;             ///< Modem bandwidth performance target configured in kHz. (4 Bytes)
    uint8_t currentMode = 0;                 ///< Current operationl modem mode (1 Byte)
    uint8_t spreadingFactor = 11;            ///< LoRa Spreading Factor configuration (typically ranges between 6 and 12). (1 Byte)
    uint8_t codingRate = 6;                  ///< Coding Rate denominator setting (e.g., value 6 yields a 4/6 coding protection overhead). (1 Byte)
    bool isInitialized = false;              ///< True if hardware was successfully validated and attached during initialization. (1 Byte)

    // The three 1-byte variables at the bottom align perfectly into a 32-bit slot, leaving minimal trailing padding.
};

/**
 * @brief Thread-isolated telemetry snapshot for UI displays and remote diagnostics.
 * @note Optimized using "largest-to-smallest" data type ordering to minimize memory padding.
 */
struct LoraStateSnapshot {
    size_t currentHistoryCount;         ///< Soft-limit threshold capping the maximum number of history records stored before auto-purge. (4 or 8 Bytes)
    uint32_t frequencyHz;               ///< RF carrier operating frequency configured in Hertz. (4 Bytes)
    uint32_t bandwidthKhz;              ///< Modem bandwidth performance target configured in kHz. (4 Bytes)
    uint32_t totalPacketsReceived;      ///< Accumulator metric track-counting successful ingress frames. (4 Bytes)
    uint32_t totalPacketsTransmitted;   ///< Accumulator metric track-counting egress frames transmitted. (4 Bytes)
    char currentModeStr[16];            ///< Human-readable string representation of the current operational modem mode. (16 Bytes)
    uint8_t spreadingFactor;            ///< LoRa Spreading Factor configuration (typically ranges between 6 and 12). (1 Byte)
    uint8_t codingRate;                 ///< Coding Rate denominator setting (e.g., value 6 yields a 4/6 coding protection overhead). (1 Byte)
    bool isInitialized;                 ///< True if hardware was successfully validated and attached during initialization. (1 Byte)

    // Elements are strictly organized by size alignment requirements to completely prevent intermittent alignment gaps.
};
