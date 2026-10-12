# FoxMesh

An open-source, international ARDF (Amateur Radio Direction Finding) timekeeping and tracking system based on programmable LoRa mesh nodes.

## About the Project

FoxMesh is a resilient, off-grid timing infrastructure designed for Radio Orienteering and ARDF events. By placing programmable FoxMesh nodes at the start, finish, and individual fox stations, the system automatically registers, collects, and synchronizes runner timestamps across the entire field using LoRa mesh technology.

### Key Features
* **Automated Checkpoints:** Nodes automatically log runner punch-ins at every station.
* **Mesh Data Sync:** No cellular network required. Timestamps are relayed from node to node back to the finish line.
* **Low Power & Cost-Effective:** Built on accessible ESP32/nRF52 and LoRa hardware (similar to MeshCom/Meshtastic).
* **Real-time Overview:** Provides race organizers with a live status of runners currently out in the field.

## Hardware Support
FoxMesh is designed to run on popular LoRa development boards:
* Heltec Automation (e.g., Wireless Stick V3, WiFi LoRa 32 V3)
* LILYGO T-Beam / T-Deck series
* RAK Wireless WisBlock modules

## Getting Started
*(Coming Soon)* Instructions on how to flash the firmware using PlatformIO and deploy the nodes in the field.

---
Project hosted under the [f0x.at](https://f0x.at) ARDF experimentation platform.
