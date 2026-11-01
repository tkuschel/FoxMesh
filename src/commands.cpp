/**
 * @file commands.cpp
 * @brief Concrete implementations of the core routing loop and all application system commands.
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */

#include "commands.hpp"
#include "config.hpp"
#include "lora.hpp"
#include "display.hpp"
#include "i2c.hpp"
#include "led.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <regex>

// Forward declarations of global pointers/variables to preserve linking hooks
extern uint32_t current_mute_time_sec;
extern TimerHandle_t silence_timer;
extern LoraController *lora_system;
extern DisplayController *oled_display;
extern i2c *i2c_bus;
extern LedController *led_system;

// The global command subsystem single instance tracking pointer
static CommandManager *global_command_manager = nullptr;

// --- Helper response routing mechanism ---
/**
 * @brief Dynamically routes console feedback strings back to the matching physical interface layer.
 */
static void respond(ResponseChannel channel, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    switch (channel) {
        case ResponseChannel::UART:
            printf("%s", buffer);
            fflush(stdout);
            break;
        case ResponseChannel::LORA:
            if (lora_system != nullptr) {
                // TODO: Wire path directly into transmitFrame once binary serialization payload handling expands
                // lora_system->transmitFrame(reinterpret_cast<uint8_t*>(buffer), strlen(buffer));
            }
            break;
        case ResponseChannel::WIFI:
            // TODO: Hook backend websocket/http daemon write buffers here
            break;
        case ResponseChannel::BLE:
            // TODO: Connect custom BLE GATT characteristic notification updater triggers here
            break;
    }
}

/**
 * @brief Strips unwanted whitespace parameters and line feed framing structures from boundary edges.
 */
static std::string trim_string(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

/**
 * @brief Validates an amateur radio callsign against strict official regulations and extracts optional SSID.
 */
static bool validate_and_parse_callsign(const std::string& raw_input, std::string& out_call, std::string& out_ssid) {
    // Separate the base callsign part and SSID part during regex evaluation to keep SSID case-sensitive
    std::string input_copy = raw_input;
    size_t dash_pos = input_copy.find('-');

    std::string base_part = (dash_pos == std::string::npos) ? input_copy : input_copy.substr(0, dash_pos);
    std::string ssid_part = (dash_pos == std::string::npos) ? "" : input_copy.substr(dash_pos + 1);

    std::transform(base_part.begin(), base_part.end(), base_part.begin(), ::toupper);

    std::regex base_pattern(R"(^([A-Z0-9][A-Z]?)([0-9])([A-Z]{1,3})$)");
    std::smatch base_matches;

    if (!std::regex_match(base_part, base_matches, base_pattern)) {
        return false;
    }

    out_call = base_matches[1].str() + base_matches[2].str() + base_matches[3].str();

    if (!ssid_part.empty()) {
        std::regex ssid_pattern(R"(^[!-~]{1,2}$)");
        if (!std::regex_match(ssid_part, ssid_pattern)) {
            return false;
        }
        out_ssid = ssid_part;
    } else {
        out_ssid = "0";
    }

    return true;
}

// --- CliCommand Base Implementation ---
CliCommand::CliCommand(std::string name, std::string description, std::string syntax)
    : m_name(std::move(name)), m_description(std::move(description)), m_syntax(std::move(syntax)) {}

const std::string &CliCommand::getName() const { return m_name; }
const std::string &CliCommand::getDescription() const { return m_description; }
const std::string &CliCommand::getSyntax() const { return m_syntax; }

// --- CommandManager Logic Implementation ---
bool CommandManager::isPrefixMatch(const std::string &input, const std::string &commandName) const {
    if (input.empty() || input.length() > commandName.length()) return false;
    return commandName.compare(0, input.length(), input) == 0;
}

void CommandManager::registerCommand(std::unique_ptr<CliCommand> command) {
    m_commands.push_back(std::move(command));
}

void CommandManager::initializeAndLoadConfig() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t nvs_storage_handle;
    if (nvs_open("foxmesh_cfg", NVS_READWRITE, &nvs_storage_handle) == ESP_OK) {
        char buffer[64] = {0};
        size_t required_size = sizeof(buffer);
        if (nvs_get_str(nvs_storage_handle, "callsign", buffer, &required_size) == ESP_OK) {
            m_callsign = buffer;
        }

        required_size = sizeof(buffer);
        std::memset(buffer, 0, sizeof(buffer));
        if (nvs_get_str(nvs_storage_handle, "ssid", buffer, &required_size) == ESP_OK) {
            m_ssid = buffer;
        }
        nvs_close(nvs_storage_handle);
    }

    if (!m_callsign.empty() && m_callsign != "NOCALL") {
        m_operationMode = OperationMode::ONLINE_MODE;
        printf("\r\n[SYSTEM] Persistent callsign found: %s-%s. Entering ONLINE chat mode.\r\n", m_callsign.c_str(), m_ssid.c_str());
    } else {
        m_operationMode = OperationMode::COMMAND_MODE;
        printf("\r\n[SYSTEM] !!! Setup incomplete !!! Call validation footprint missing.\r\n");
        printf("[SYSTEM] Locked inside CLI. Please execute configuration: setcall <YOUR_CALLSIGN>[-SSID]\r\n");
    }
}

void CommandManager::setCallsign(const std::string &newCallsign, const std::string &newSsid) {
    m_callsign = newCallsign;
    m_ssid = newSsid;
    nvs_handle_t nvs_storage_handle;
    if (nvs_open("foxmesh_cfg", NVS_READWRITE, &nvs_storage_handle) == ESP_OK) {
        nvs_set_str(nvs_storage_handle, "callsign", newCallsign.c_str());
        nvs_set_str(nvs_storage_handle, "ssid", newSsid.c_str());
        nvs_commit(nvs_storage_handle);
        nvs_close(nvs_storage_handle);
    }
}

void CommandManager::setOperationMode(OperationMode newMode) {
    m_operationMode = newMode;
    if (m_operationMode == OperationMode::ONLINE_MODE) {
        respond(ResponseChannel::UART, "\r\n[MODE] Switched to ONLINE chat interface. Default prompt target: @%s\r\n", m_active_target.c_str());
        respond(ResponseChannel::UART, "@%s> ", m_active_target.c_str());
    } else {
        respond(ResponseChannel::UART, "\r\n[MODE] Switched back to interactive COMMAND execution layer.\r\n");
        respond(ResponseChannel::UART, "%s # ", Config::PROJECT_TITLE);
    }
}

void CommandManager::processLine(const std::string& rawLine, ResponseChannel channel) {
    std::string sanitized_line = trim_string(rawLine);
    if (sanitized_line.empty()) {
        if (m_operationMode == OperationMode::ONLINE_MODE) {
            respond(channel, "@%s> ", m_active_target.c_str());
        }
        return;
    }

    // Escape sequence handler forcing a terminal exit out of chat structures into standard CLI configuration profiles
    if (sanitized_line == "+++") {
        setOperationMode(OperationMode::COMMAND_MODE);
        respond(channel, "%s # ", Config::PROJECT_TITLE);
        return;
    }

    if (m_operationMode == OperationMode::ONLINE_MODE) {
        handleOnlineChatMode(sanitized_line, channel);
        return;
    }

    std::stringstream ss(sanitized_line);
    std::string token;
    std::vector<std::string> tokens;

    while (ss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return;
    respond(channel, "\r\n");

    std::string firstInput = tokens[0];

    if (firstInput == "?" || firstInput == "help") {
        printGlobalHelp(channel);
        respond(channel, "%s # ", Config::PROJECT_TITLE);
        return;
    }

    for (const auto &cmd : m_commands) {
        if (isPrefixMatch(firstInput, cmd->getName())) {
            std::vector<std::string> args(tokens.begin() + 1, tokens.end());

            if (!args.empty() && args[0] == "?") {
                respond(channel, "Command : %s\r\n", cmd->getName().c_str());
                respond(channel, "Desc    : %s\r\n", cmd->getDescription().c_str());
                respond(channel, "Syntax  : %s\r\n", cmd->getSyntax().c_str());
                respond(channel, "%s # ", Config::PROJECT_TITLE);
                return;
            }

            cmd->execute(args, channel);
            if (m_operationMode == OperationMode::COMMAND_MODE) {
                respond(channel, "%s # ", Config::PROJECT_TITLE);
            }
            return;
        }
    }

    respond(channel, "Unknown command: '%s'. Type '?' for options.\r\n", firstInput.c_str());
    respond(channel, "%s # ", Config::PROJECT_TITLE);
}

void CommandManager::handleOnlineChatMode(const std::string &rawLine, ResponseChannel channel) {
    std::string broadcast_message = rawLine;

    // Evaluate target extraction routines if current terminal line explicitly starts with the prefix character '@'
    if (rawLine[0] == '@') {
        size_t separator_index = rawLine.find_first_of(" :");
        if (separator_index != std::string::npos) {
            // Slicing targeted operational scope node out of the active terminal frame
            m_active_target = rawLine.substr(1, separator_index - 1);

            size_t message_start_index = separator_index;
            if (rawLine[message_start_index] == ':') {
                message_start_index++;
            }
            broadcast_message = trim_string(rawLine.substr(message_start_index));
        } else {
            // Prompt assignment shorthand (e.g. '@oe1kbc' isolates the specific call without sending an empty payload)
            m_active_target = rawLine.substr(1);
            respond(channel, "\r\n[Target Locked] Scope context updated to: @%s\r\n", m_active_target.c_str());
            respond(channel, "@%s> ", m_active_target.c_str());
            return;
        }
    }

    if (broadcast_message.empty()) {
        respond(channel, "@%s> ", m_active_target.c_str());
        return;
    }

    // Packet safety check guarding the payload limit specified by system transmission bounds
    if (broadcast_message.length() > 160) {
        respond(channel, "\r\n[ERROR] Core frame rejected! Message block length exceeds 160 bytes limitation limit (%zu).\r\n", broadcast_message.length());
        respond(channel, "@%s> ", m_active_target.c_str());
        return;
    }

    // Dispatch processed structural payload to peripheral hardware layers
    respond(channel, "\r\n[TX Packet] (%s-%s -> @%s): %s\r\n", m_callsign.c_str(), m_ssid.c_str(), m_active_target.c_str(), broadcast_message.c_str());

    if (lora_system != nullptr) {
        // TODO: Map serial string buffers directly into active network routing mesh layers
    }

    respond(channel, "@%s> ", m_active_target.c_str());
}

void CommandManager::printGlobalHelp(ResponseChannel channel) const {
    respond(channel, "Available commands:\r\n");
    for (const auto &cmd : m_commands) {
        respond(channel, "  %-18s - %s\r\n", cmd->getSyntax().c_str(), cmd->getDescription().c_str());
    }
}

// --- Concrete System Commands Definitions ---

class SetCallsignCommand : public CliCommand {
public:
    SetCallsignCommand() : CliCommand("setcall", "Configure persistent radio network callsign identity identifier.", "setcall <callsign>[-ssid]") {}

    void execute(const std::vector<std::string>& arguments, ResponseChannel channel) override {
        if (arguments.empty()) {
            respond(channel, "Usage error: Please specify parameter profile as 'setcall <callsign>[-ssid]'\r\n");
            return;
        }

        std::string parsed_call = "";
        std::string parsed_ssid = "0";

        if (!validate_and_parse_callsign(arguments[0], parsed_call, parsed_ssid)) {
            respond(channel, "Error: Invalid callsign format scheme!\r\n");
            respond(channel, "Required standard syntax: <1-2 Prefix><1 Digit><1-3 Suffix>[-<1-2 SSID>]\r\n");
            respond(channel, "Examples of valid input profiles: OE6GC, oe3tkt-3, KW4NZ-42, OE1XA-2\r\n");
            return;
        }

        CommandManager *manager = get_command_manager_instance();
        manager->setCallsign(parsed_call, parsed_ssid);

        respond(channel, "Success: Identity mapping verified and written to flash!\r\n");
        respond(channel, "  Callsign : %s\r\n", parsed_call.c_str());
        respond(channel, "  SSID     : %s\r\n", parsed_ssid.c_str());

        manager->setOperationMode(OperationMode::ONLINE_MODE);
    }
};

class OnlineModeCommand : public CliCommand {
public:
    OnlineModeCommand() : CliCommand("online", "Switch execution context from local terminal parsing to chat message distribution mode.", "online") {}

    void execute(const std::vector<std::string> &, ResponseChannel channel) override {
        CommandManager *manager = get_command_manager_instance();
        if (manager->getCallsign().empty() || manager->getCallsign() == "NOCALL") {
            respond(channel, "Error: Execution halt. Please register your active system callsign identity first using 'setcall'.\r\n");
            return;
        }
        manager->setOperationMode(OperationMode::ONLINE_MODE);
    }
};

class VersionCommand : public CliCommand {
public:
    VersionCommand() : CliCommand("version", "Display project title and version.", "version [f|full]") {}

    void execute(const std::vector<std::string> &arguments, ResponseChannel channel) override {
        if (!arguments.empty()) {
            std::string sub = arguments[0];
            if (sub.compare(0, sub.length(), "full") == 0) {
                respond(channel, "%s System Matrix - Version: %s (Target: ESP32, IDF: %s)\r\n",
                        Config::PROJECT_TITLE, Config::PROJECT_VERSION, esp_get_idf_version());
                return;
            }
        }
        respond(channel, "%s System - Running Version: %s Tag: " GIT_TAG "\r\n", Config::PROJECT_TITLE, Config::PROJECT_VERSION);
        respond(channel, "Build Date: " __DATE__ ", Git-Hash: " GIT_HASH "\r\n");
    }
};

class StatusCommand : public CliCommand {
public:
    StatusCommand() : CliCommand("status", "Print internal hardware statistics and heap metrics.", "status") {}

    void execute(const std::vector<std::string>&, ResponseChannel channel) override {
        respond(channel, "--- %s Status ---\r\n", Config::PROJECT_TITLE);
        respond(channel, "Free Internal Heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
        respond(channel, "Mute Timer Period : %lu seconds\r\n", (unsigned long)current_mute_time_sec);
        respond(channel, "Target CPU Board  : ESP32\r\n");
        respond(channel, "ESP-IDF Version   : %s\r\n", esp_get_idf_version());

        if (lora_system != nullptr) {
            LoraStateSnapshot snapshot = lora_system->getSnapshot();
            respond(channel, "LoRa Hardware     : %s\r\n", snapshot.isInitialized ? "INITIALIZED" : "FAILED / UNINITIALIZED");
            respond(channel, "LoRa Transceiver  : Mode [%s]\r\n", snapshot.currentModeStr);
            respond(channel, "LoRa Frequency    : %lu Hz\r\n", (unsigned long)snapshot.frequencyHz);
            respond(channel, "LoRa Bandwidth    : %lu kHz\r\n", (unsigned long)snapshot.bandwidthKhz);
            respond(channel, "LoRa SpreadingFact: SF%u\r\n", snapshot.spreadingFactor);
            respond(channel, "LoRa Coding Rate  : 4/%u\r\n", snapshot.codingRate);
            respond(channel, "Packets Received  : %lu\r\n", (unsigned long)snapshot.totalPacketsReceived);
            respond(channel, "Packets Transmit  : %lu\r\n", (unsigned long)snapshot.totalPacketsTransmitted);
            respond(channel, "History In Memory : %zu Packets\r\n", snapshot.currentHistoryCount);
        } else {
            respond(channel, "LoRa Snapshot     : Error (LoraController Handle uninitialized)\r\n");
        }
    }
};

class MuteCommand : public CliCommand {
public:
    MuteCommand() : CliCommand("mute", "Set custom logging runtime suppression interval.", "mute [seconds]") {}

    void execute(const std::vector<std::string>& arguments, ResponseChannel channel) override {
        int parsed_time = 0; // Default: 'mute' without arguments equals 'mute 0'
        if (!arguments.empty()) {
            parsed_time = atoi(arguments[0].c_str());
        }

        if (parsed_time >= 0 && parsed_time <= 3600) {
            current_mute_time_sec = static_cast<uint32_t>(parsed_time);

            if (current_mute_time_sec == 0) {
                if (silence_timer != nullptr) {
                    xTimerStop(silence_timer, pdMS_TO_TICKS(10));
                }
                respond(channel, "Success: Infinite mute activated. Debug logs are permanently suppressed.\r\n");
            } else {
                uint32_t timeout_ms = current_mute_time_sec * 1000;
                if (silence_timer != nullptr) {
                    xTimerChangePeriod(silence_timer, pdMS_TO_TICKS(timeout_ms), pdMS_TO_TICKS(10));
                }
                respond(channel, "Success: Mute timer interval updated to %lu seconds.\r\n", (unsigned long)current_mute_time_sec);
            }
        } else {
            respond(channel, "Error: Please provide a valid timeout between 0 (infinite) and 3600 seconds.\r\n");
        }
    }
};

class BlinkCommand : public CliCommand {
public:
    BlinkCommand() : CliCommand("blink", "Set LED profile interval and pulse width in ms.", "blink <interval_ms> <pulse_width_ms>") {}

    void execute(const std::vector<std::string>& arguments, ResponseChannel channel) override {
        if (arguments.size() < 2) {
            respond(channel, "Usage error: Please specify parameters as 'blink <interval_ms> <pulse_width_ms>'\r\n");
            return;
        }
        uint32_t interval = strtoul(arguments[0].c_str(), nullptr, 10);
        uint32_t pulse = strtoul(arguments[1].c_str(), nullptr, 10);

        if (interval > 0 && pulse > 0 && pulse < interval) {
            if (led_system != nullptr) {
                LedController::BlinkProfile customProfile;
                customProfile.intervalMs = interval;
                customProfile.pulseWidthMs = pulse;
                led_system->updateProfile(customProfile);
                respond(channel, "Success: LED profile updated. Interval: %lu ms, Pulse: %lu ms.\r\n", (unsigned long)interval, (unsigned long)pulse);
            } else {
                respond(channel, "Error: LedController handle is uninitialized.\r\n");
            }
        } else {
            respond(channel, "Error: Invalid boundaries. Pulse width must be smaller than total interval.\r\n");
        }
    }
};

class FreqCommand : public CliCommand {
public:
    FreqCommand() : CliCommand("freq", "Set dynamic radio frequency targets.", "freq <Hz>") {}

    void execute(const std::vector<std::string>& arguments, ResponseChannel channel) override {
        if (arguments.empty()) {
            respond(channel, "Usage error: Please specify parameters as 'freq <frequency_in_hz>'\r\n");
            return;
        }
        uint32_t target_freq_hz = strtoul(arguments[0].c_str(), nullptr, 10);
        bool valid_433 = (target_freq_hz >= 430000000 && target_freq_hz <= 450000000);
        bool valid_868 = (target_freq_hz >= 863000000 && target_freq_hz <= 870000000);

        if (valid_433 || valid_868) {
            if (lora_system != nullptr) {
                lora_system->setFrequency(target_freq_hz);
                respond(channel, "Success: Frequency shifted to %lu Hz.\r\n", (unsigned long)target_freq_hz);
            } else {
                respond(channel, "Error: LoraController handle is uninitialized.\r\n");
            }
        } else {
            respond(channel, "Error: Frequency out of boundaries 430-450 / 863-870 MHz band limits.\r\n");
        }
    }
};

class BandwidthCommand : public CliCommand {
public:
    BandwidthCommand() : CliCommand("bw", "Set dynamic LoRa modem bandwidth selections.", "bw <62|125|250|500>") {}

    void execute(const std::vector<std::string>& arguments, ResponseChannel channel) override {
        if (arguments.empty()) {
            respond(channel, "Usage error: Please specify parameter as 'bw <62|125|250|500>'\r\n");
            return;
        }
        uint32_t target_bw = atoi(arguments[0].c_str());
        if (target_bw == 62 || target_bw == 125 || target_bw == 250 || target_bw == 500) {
            if (lora_system != nullptr) {
                lora_system->setBandwidth(target_bw);
                respond(channel, "Success: Dispatched bandwidth configuration update to %lu kHz.\r\n", (unsigned long)target_bw);
            } else {
                respond(channel, "Error: LoraController handle is uninitialized.\r\n");
            }
        } else {
            respond(channel, "Error: Invalid choice. Supported bandwidth options are: 62, 125, 250, 500.\r\n");
        }
    }
};

class OledCommand : public CliCommand {
public:
    OledCommand() : CliCommand("oled", "Print custom message payloads across explicit display lines.", "oled <line> <text>") {}

    void execute(const std::vector<std::string> &arguments, ResponseChannel channel) override {
        if (arguments.empty()) {
            respond(channel, "Usage error: Please specify parameters as 'oled <line_id> <text_message>'\r\n");
            return;
        }
        uint32_t targetLine = atoi(arguments[0].c_str());
        std::string textMessage;
        if (arguments.size() == 1) {
            textMessage = "";
        } else {
            for (size_t i = 1; i < arguments.size(); ++i) {
                textMessage += arguments[i] + (i == arguments.size() - 1 ? "" : " ");
            }
        }
        if (targetLine <= 7) {
            if (oled_display != nullptr) {
                oled_display->printMessage(static_cast<uint8_t>(targetLine), textMessage.c_str());
                respond(channel, "Success: Dispatched text '%s' to OLED line %lu.\r\n", textMessage.c_str(), (unsigned long)targetLine);
            } else {
                respond(channel, "Error: DisplayController handle is uninitialized.\r\n");
            }
        } else {
            respond(channel, "Error: Target line must be within the range 0-7.\r\n");
        }
    }
};

class I2cCommand : public CliCommand {
public:
    I2cCommand() : CliCommand("i2c", "Scan the configured I2C bus infrastructure for active responder nodes.", "i2c") {}

    void execute(const std::vector<std::string>&, ResponseChannel channel) override {
        if (i2c_bus != nullptr) {
            respond(channel, "Scanning I2C Bus... \r\n");
            i2c_bus->scan();
        } else {
            respond(channel, "Error: I2C Bus context handle is uninitialized.\r\n");
        }
    }
};

class TestDisplayCommand : public CliCommand {
public:
    TestDisplayCommand() : CliCommand("test-display", "Run a comprehensive visual test suite across the connected OLED matrix panels.", "test-display") {}

    void execute(const std::vector<std::string>&, ResponseChannel channel) override {
        respond(channel, "[CLI] Triggering asynchronous OLED font verification...\n");
        run_display_font_test(oled_display);
        respond(channel, "[CLI] Test completed.\n");
    }
};

// --- C-Bridge Interfacing Methods implementation ---
void initialize_commands_system() {
    if (global_command_manager != nullptr) return;
    global_command_manager = new CommandManager();
    global_command_manager->initializeAndLoadConfig();

    // Priority Chain Setup: First registered command takes execution shorthand precedence!
    global_command_manager->registerCommand(std::make_unique<SetCallsignCommand>());
    global_command_manager->registerCommand(std::make_unique<OnlineModeCommand>());
    global_command_manager->registerCommand(std::make_unique<VersionCommand>());
    global_command_manager->registerCommand(std::make_unique<StatusCommand>());
    global_command_manager->registerCommand(std::make_unique<MuteCommand>());
    global_command_manager->registerCommand(std::make_unique<BlinkCommand>());
    global_command_manager->registerCommand(std::make_unique<FreqCommand>());
    global_command_manager->registerCommand(std::make_unique<BandwidthCommand>());
    global_command_manager->registerCommand(std::make_unique<OledCommand>());
    global_command_manager->registerCommand(std::make_unique<I2cCommand>());
    global_command_manager->registerCommand(std::make_unique<TestDisplayCommand>());
}

void process_command(const char *cmd, ResponseChannel channel) {
    if (global_command_manager == nullptr) {
        initialize_commands_system();
    }
    global_command_manager->processLine(std::string(cmd), channel);
}

CommandManager *get_command_manager_instance() {
    if (global_command_manager == nullptr) {
        initialize_commands_system();
    }
    return global_command_manager;
}
