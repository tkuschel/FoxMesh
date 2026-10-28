# FoxMesh Code Style and Coding Standards

This document defines the strict architectural and structural coding standards for **FoxMesh**. Every contributor must adhere to these rules to guarantee clean, deterministic, and highly responsive firmware code.

---

## 1. General Principles

* **Language:** All source code, variable names, class names, file names, and comments **must be written entirely in English**.
* **Modern C++ Execution:** We target pure C++ implementation architectures utilizing **ESP-IDF v6.0+ (PlatformIO platform v7.x)**.
* **Framework Restrictions:** The Arduino framework layer is **strictly prohibited**. Only native ESP-IDF APIs are permitted.
* **No CPU Polling:** Code must be completely event-driven or asynchronous. Traditional busy-waiting or repetitive polling loops inside worker tasks are banned. Use FreeRTOS blocking mechanisms (Queues, Notifications, Semaphores) instead.

---

## 2. File and Header Formats

### 2.1 File Inclusions
* Every header file (`.hpp`) must start with `#pragma once` to prevent multi-include compilation loops. Traditional `#ifndef` macro guards are deprecated.

### 2.2 Standard SPDX File Header
Every source file (`.hpp`, `.cpp`) must feature a compact, Doxygen-compliant documentation block identifying the file scope, the author **OM Tom (OE3TKT)**, and the **MIT License** via SPDX shorthand markers.

```cpp
/**
 * @file filename.hpp
 * @brief Concise one-line description of the file's primary hardware/software layer
 * @author OM Tom (OE3TKT)
 *
 * @license SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 OM Tom (OE3TKT)
 * For the full license terms, see the LICENSE file in the project root.
 */
```

---

## 3. Naming Conventions

To guarantee readability when scanning source text, we enforce strict naming rules across distinct entity spaces:

| Entity | Case Style | Example | Notes |
| :--- | :--- | :--- | :--- |
| **Classes & Structs** | `PascalCase` | `LedController`, `BlinkProfile` | Clear noun structures. |
| **Methods & Functions** | `camelCase` | `startTask()`, `updateProfile()` | Action-oriented verbs. |
| **Local Variables** | `camelCase` | `remainingLowTimeMs`, `bufferIndex` | Descriptive names. |
| **Private Member Variables** | `m_camelCase` | `m_gpioPin`, `m_taskHandle` | Prefixed with `m_` for instant scoping. |
| **Compile-Time Constants** | `UPPER_CASE` | `BLINK_INTERVAL_MS`, `LED_ON_MS` | Declared via `constexpr` where possible. |

---

## 4. FreeRTOS & Thread-Safety Rules

Because FoxMesh acts as an international real-time tracking network, core memory regions must be isolated against multi-core Race Conditions:

### 4.1 Cross-Thread Communication
* Avoid simple `bool` flags or global un-mutexed variables for controlling background tasks.
* To wake up or re-configure a sleeping/blocking task immediately, use **FreeRTOS Task Notifications** (`xTaskNotifyGive` / `ulTaskNotifyWait`).
* For heavy data passing, use thread-safe FreeRTOS `QueueHandle_t` pipelines.

### 4.2 Core Pinning Allocation
* **Core 0:** Reserved for user-facing, asynchronous interactive modules like the Serial Console interface (`CLI`).
* **Core 1:** Strictly reserved for timing-critical tracking execution layers, high-priority Interrupt Service Routines (ISRs), and the **Semtech LoRa Mesh network layer**.

---

## 5. Compiler Warnings and Type Safety

Our `platformio.ini` profile forces highly aggressive compiler strictness using GCC. Code must be designed to satisfy the following rigid parameters:

### 5.1 No Implicit Narrowing (`-Wconversion`)
Implicit down-casting of numerical boundaries will drop execution blocks on the spot. You must enforce clear, deterministic size markers.
* **Bad:** `uint8_t shortInterval = intervalMs;`
* **Good:** `uint8_t shortInterval = static_cast<uint8_t>(intervalMs);`

### 5.2 No Variable Shadowing (`-Wshadow`)
A localized function-scope configuration parameter must never mask out an upper-tier variable state. Ensure completely unique identifier tokens across inner blocks.

### 5.3 Mandatory Explicit Type Casts
When passing numbers into standard FreeRTOS macro conversions (like `pdMS_TO_TICKS()`), ensure calculation metrics are cast beforehand if values are mixed signed/unsigned types.

---

## 6. Error Handling and System Stability

To guarantee continuous operation during field deployment, error conditions must be handled strictly according to their severity:

### 6.1 Fatal Critical Setup Errors
* The use of Espressif's `ESP_ERROR_CHECK()` macro is **only allowed during hardware initialization** (e.g., inside `app_main` or bus allocation wrappers).
* If a critical bus initialization fails on boot, a hardware panic/reboot via abort is acceptable to prevent running undefined node configurations.

### 6.2 Dynamic Runtime Errors
* Any error occurring during active field operations (e.g., I2C sensor timeouts, frame drop glitces, or temporary LoRa transmission failures) **must never use `ESP_ERROR_CHECK()`**.
* Runtime anomalies must be caught defensively by checking return values (`esp_err_t == ESP_OK`), resolving them gracefully, and outputting explicit standard logging metrics via `ESP_LOGE`.

---

## 7. Doxygen Documentation Requirements

All public APIs, public structures, and method signatures within header files must include Javadocs-style `/** ... */` annotations so that documentation engines can generate an automated index map.

```cpp
/**
 * @brief Dynamic method modifying a running task profile state.
 * @param profile Input mapping parameters for the new execution layer.
 */
void updateProfile(const BlinkProfile& profile);
```

## 8. Git Branching and Commit Standards

To ensure traceability and maintain a clean repository history, FoxMesh strictly follows a standardized branching and commit convention based on *Conventional Commits*.

### 8.1 Branch Naming Convention
Developers must prefix every newly created Git branch with a specific scope identifier followed by a slash (`/`), the Gitea issue number, and a short description.

* **Format:** `<type>/<issue-number>-<short-description>`
* **Example:** `docs/123-readme-update` or `feat/45-lora-mesh-ack`

### 8.2 Scope Identifiers
The following prefixes are permitted for branch creation and commit messages:

* `feat/` (or `feature/`): Introduction of new hardware or software functionalities.
* `fix/` (or `bugfix/`): Resolution of bugs, runtime stability issues, or crashes.
* `docs/`: Exclusively reserved for pure documentation updates (Readmes, Doxygen, Wiki).
* `refactor/`: Code reorganization or optimization without changing external behavior.
* `chore/`: Updates to the build system, `platformio.ini`, CI/CD pipelines, or external dependencies.

### 8.3 Commit and Issue Automation
Every commit message should start with the lowercase prefix (e.g., `docs: description`) and must include the targeted Gitea issue number in parentheses at the end to allow automated tracking and timeline linkage:

```bash
git commit -m "docs: added docker installation guide (#123)"
```

When submitting a Pull Request (PR) to the `main` branch, developers must use standard closure keywords (`closes #123`, `resolves #123`) within the PR description to ensure Gitea automatically resolves the ticket upon integration.
