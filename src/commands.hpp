/**
 * @file commands.hpp
 * @brief Unified interface and class declarations for the object-oriented CLI system.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <list>
#include "esp_err.h"

/**
 * @brief Operation states tracking whether the node is in command or data mode.
 */
enum class OperationMode {
    COMMAND_MODE, ///< Classic Hayes / Command Line Interface mode active.
    ONLINE_MODE   ///< Transparent interactive chat mode for immediate message dispatch.
};

/**
 * @brief Communication interfaces over which a command can be triggered and replied to.
 */
enum class ResponseChannel {
    UART, ///< Standard local serial USB interface.
    LORA, ///< Wireless LoRa RF transceiver link.
    WIFI, ///< Network sockets or WebInterface.
    BLE   ///< Bluetooth Low Energy GATT characteristic.
};

/**
 * @brief Abstract base class representing a single executable system command.
 */
class CliCommand {
protected:
    std::string m_name;        /**< Full primary name of the command (e.g., "version"). */
    std::string m_description; /**< Comprehensive help text descriptive block. */
    std::string m_syntax;      /**< Detailed argument syntax usage signature blueprint. */

public:
    CliCommand(std::string name, std::string description, std::string syntax);
    virtual ~CliCommand() = default;

    const std::string &getName() const;
    const std::string &getDescription() const;
    const std::string &getSyntax() const;

    /**
     * @brief Forces the command name to be fully typed out without abbreviations.
     * @return true if strict matching is required, false otherwise.
     */
    virtual bool isStrictMatch() const { return false; }

    /**
     * @brief Core execution trigger invoked when a command match occurs.
     * @param arguments Vector containing tokenized string parameters.
     * @param channel The source channel where the command originated.
     */
    virtual void execute(const std::vector<std::string> &arguments, ResponseChannel channel) = 0;
};

/**
 * @brief Orchestrates parsing, abbreviation pathing alignments, and execution dispatching.
 */
class CommandManager {
private:
    std::vector<std::unique_ptr<CliCommand>> m_commands;
    OperationMode m_operationMode = OperationMode::COMMAND_MODE;
    std::string m_callsign = "";
    std::string m_ssid = "0";
    std::string m_active_target = "all";

    /**
     * @brief Checks if a short input string matches the prefix of a full command name.
     */
    bool isPrefixMatch(const std::string &input, const std::string &commandName) const;

    /**
     * @brief Parses and processes data packet traffic when in transparent online chat mode.
     */
    void handleOnlineChatMode(const std::string &rawLine, ResponseChannel channel);

public:
    CommandManager() = default;

    /**
     * @brief Registers a new command class inside the local priority stack tracking engine.
     * @note Insert high-priority commands first to secure shorter abbreviation rights.
     */
    void registerCommand(std::unique_ptr<CliCommand> command);

    /**
     * @brief Processes a raw console entry line string tokenizing and evaluating matching contexts.
     * @param rawLine The unmodified line input fetched from input streams.
     * @param channel Transmission channel specifying the source origin of the raw line.
     */
    void processLine(const std::string &rawLine, ResponseChannel channel = ResponseChannel::UART);

    /**
     * @brief Outputs the complete global help overview text map block.
     * @param channel Target interface channel where the help menu should be routed.
     */
    void printGlobalHelp(ResponseChannel channel) const;

    /**
     * @brief Mounts non-volatile storage flash blocks and loads configuration keys.
     */
    void initializeAndLoadConfig();

    /**
     * @brief Atomically updates and persists the operational callsign into local flash profiles.
     * @param newCallsign The uppercase clean callsign.
     * @param newSsid The case-sensitive raw SSID string.
     */
    void setCallsign(const std::string &newCallsign, const std::string &newSsid);

    /**
     * @brief Fetches the current operational callsign profile.
     */
    const std::string &getCallsign() const { return m_callsign; }

    /**
     * @brief Fetches the current operational SSID profile.
     */
    const std::string &getSsid() const { return m_ssid; }

    /**
     * @brief Fetches the currently selected runtime operation state.
     */
    OperationMode getOperationMode() const { return m_operationMode; }

    /**
     * @brief Updates the operating system tracking state and repaints matching console prompt contexts.
     */
    void setOperationMode(OperationMode newMode);

    /**
     * @brief Fetches the current targeted routing destination alias.
     */
    const std::string &getActiveTarget() const { return m_active_target; }
};

/**
 * @brief One-time global system startup initialization and registration mapping.
 */
void initialize_commands_system();

/**
 * @brief Central entry-point for external files to forward raw inputs into the parsing pipeline.
 * @param cmd Null-terminated character array payload.
 * @param channel Interface route tracking token matching the incoming stream origin.
 */
void process_command(const char *cmd, ResponseChannel channel = ResponseChannel::UART);

/**
 * @brief Securely exports the memory handle referencing the singleton orchestrator instance.
 */
CommandManager *get_command_manager_instance();
