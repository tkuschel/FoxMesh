#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class DisplayController {
public:
    // Structure for the thread-safe communication queue
    struct DisplayMessage {
        char text[32]; // Buffer for short status messages (e.g., "LED ON", "Mesh Packet")
        int row;       // Target row on the screen (e.g., 0 to 3)
    };

    DisplayController();
    virtual ~DisplayController() = default;

    // Deleted copy constructors for hardware safety
    DisplayController(const DisplayController&) = delete;
    DisplayController& operator=(const DisplayController&) = delete;

    // Initializes I2C and spawns the background worker task on Core 0
    void startTask(uint32_t priority = 4);

    // Thread-safe public method that other tasks call to print text
    bool printMessage(const char* text, int row);

private:
    static const char* TAG;
    QueueHandle_t m_msgQueue;

    void initI2C();
    void initSSD1306();
    void sendCommand(uint8_t command);

    // FreeRTOS C++ bridging mechanisms
    static void taskTrampoline(void* pvParameters);
    void run();
};

#endif // DISPLAY_HPP
