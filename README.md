# FoxMesh

An open-source, international ARDF (Amateur Radio Direction Finding) timekeeping and tracking system based on programmable LoRa mesh nodes.

## About the Project

FoxMesh is a resilient, off-grid timing infrastructure designed for Radio Orienteering and ARDF events. By placing programmable FoxMesh nodes at the start, finish, and individual fox stations, the system automatically registers, collects, and synchronizes runner timestamps across the entire field using LoRa mesh technology.

### Key Features
* **Automated Checkpoints:** Logs runner punch-ins automatically at every station.
* **Mesh Data Sync:** Operates entirely without cellular networks, relaying timestamps from node to node back to the finish line.
* **Low Power & Cost-Effective:** Built on highly accessible ESP32/nRF52 and LoRa hardware (similar to MeshCom or Meshtastic).
* **Real-Time Overview:** Provides race organizers with a live status monitor of runners currently out in the field.

### Technical Architecture
* **Native ESP-IDF:** Developed using the pure `espressif32` platform (v7.x release branch) without the overhead of the Arduino framework.
* **Event-Driven Multitasking:** Built from the ground up with thread-safe, dynamically expandable libraries. Avoids CPU-heavy polling for subsystems like CLI, GNSS, or I2C.

## Hardware Support
> ⚠️ **Note:** Hardware support is currently limited as the project is under active development.

The firmware is designed to target popular LoRa development ecosystems, with primary testing and core drivers focusing on:
* **Heltec Automation:** Wireless Stick V3, WiFi LoRa 32 V3
* **LILYGO:** T-Beam & T-Deck series
* **RAK Wireless:** WisBlock modules

---

## 🌐 Project Mirrors / Hosting
To ensure high availability and decentralization, this project is maintained across multiple platforms:

* **Primary Self-Hosted Repo (Gitea):** [git.kuschel.at/tom/FoxMesh](https://git.kuschel.at/tom/FoxMesh)
* **Mirror Repo (GitHub):** [github.com/tkuschel/FoxMesh](https://github.com/tkuschel/FoxMesh)

*Note: Both repositories are automatically kept synchronized. You can clone, open issues, or contribute on either platform.*

*Contributions and hardware abstraction layer (HAL) testing for specific boards are highly welcome!*

## Getting Started
*(Coming Soon)* Instructions on how to flash the firmware using PlatformIO and deploy the nodes in the field.

## Developers
Please refer to the documentation pages under `docs/developers/` for architecture deep-dives and contribution guidelines.

---
Project hosted under the [f0x.at](https://f0x.at) ARDF experimentation platform.
