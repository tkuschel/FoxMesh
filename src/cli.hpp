/**
 * @file cli.hpp
 * @brief Interactive asynchronous Command Line Interface for FoxMesh
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include "i2c.hpp"
#include "led.hpp"
#include "display.hpp"
#include "lora.hpp"

/**
 * @brief Store peripheral, display, and hardware tracking references for CLI access.
 * @param bus_pointer Pointer to the active tracking system I2C master instance.
 * @param led_pointer Pointer to the active tracking system status LED instance.
 * @param display_pointer Pointer to the active tracking system OLED instance.
 */
void cli_init(i2c* bus_pointer, LedController* led_pointer, DisplayController* display_pointer);

/**
 * @brief Connects the asynchronous LoRa subsystem reference context back onto the interface layer.
 * @param lora_pointer Active LoRa transceiver module controller layer instance.
 */
void cli_init_lora(LoraController* lora_pointer);

/**
 * @brief Registers the background tasks and setup routines for core runtime execution.
 */
void start_cli_task(void);

/**
 * @brief Tokenizes and evaluates incoming control frames from the hardware terminal.
 * @param cmd Null-terminated character string containing the raw user command execution layer.
 */
void process_command(const char* cmd);
