# LoRa Mesh & Device Settings (Austria / EU)

This document provides a comprehensive overview of the **MeshCom** parameters, frequency structures, and alternative LoRa options according to the official Austrian (**ÖVSV**) 70cm band plan.

---

## 📻 Part 1: MeshCom Frequency & Required Parameters in the 70cm Band (433 MHz)

In Austria and the entire D-A-CH region, the amateur radio project **MeshCom** (MeshCom 4.0) operates on the primary standard frequency of **433.175 MHz**.

> ⚠️ **Requirement:** Since MeshCom runs within the 70cm amateur radio band, a **valid amateur radio license** and a personal **call sign** are strictly required to participate.

### LoRa & System Parameters
To successfully communicate with the Austrian and European mesh network, your LoRa modulation parameters must match the standard exactly. In the current MeshCom firmware, these are set automatically as soon as you configure your country/region code to **EU** (or **EU8**):

| Parameter | Value | Description / Command |
| :--- | :--- | :--- |
| **Frequency** | **433.175 MHz** | Main frequency for data and text messages |
| **Bandwidth (BW)** | **250 kHz** | Signal width in the spectrum |
| **Spreading Factor (SF)** | **11** | Defines the chip-to-symbol rate |
| **Coding Rate (CR)** | **4/6** | Error correction rate |
| **LoRa Profile** | **MediumSlow** | The standardized transmission profile |
| **Track Frequency** | **433.775 MHz** | Optional secondary frequency for pure position data (LoRa-APRS) |

> 🔍 **Important Note on SF11 vs. SF12:**
> * **SF11 (BW 250 kHz)** is the hardcoded standard for the primary MeshCom text/data channel (433.175 MHz). It uses the optimized "MediumSlow" profile to ensure a balanced time-on-air.
> * **SF12 (BW 125 kHz)** is *only* utilized on the separate **Tracker Frequency (433.775 MHz)** for LoRa-APRS position telemetry packets. Manually forcing the main channel to SF12 will break compatibility with the primary MeshCom grid and regional gateways.

### Device Configuration
When setting up your node for the first time (via the MeshCom Web Installer or the smartphone app), you must manually configure the following parameters:

*   **Call Sign (`--setcall`):** Your unique amateur radio call sign combined with an APRS-compliant SSID (e.g., `OE3XXX-9` for mobile nodes or `OE1XXX-12` for internet gateways).
*   **Country Code (`--setctry`):** Must be set to **EU** to automatically apply the correct LoRa profile.
*   **Fixed Coordinates (`--setlat`, `--setlon`, `--setalt`):** If your node lacks a GPS module or functions as a static home gateway, you must enter its latitude, longitude, and altitude manually so it displays correctly on the official ÖVSV MeshCom map.

> 🌐 **For Internet Gateways:** You will additionally need to provide your local Wi-Fi credentials via `--setssid [Name]` / `--setpwd [Password]` and enable the gateway routing using `--gateway on` and `--setinet`.

---

## 🔄 Part 2: Alternative Frequencies in the 433 MHz Band

According to the official **70cm band plan of the ÖVSV** (Austrian Experimental Transmitter Association), there are alternative options within the 433 MHz range. However, a strict distinction must be made between **Amateur Radio services** (MeshCom) and **license-free ISM use** (such as Meshtastic).

### 1. Options Within MeshCom (Amateur Radio)
Because a mesh network relies on all nodes sharing the same channel to forward packets, changing your frequency will disconnect you from the main Austrian network and its gateways. However, for isolated, experimental, or local group networks, the band plan offers structural alternatives:

*   **The "All Modes" Segment (434.000 – 434.9875 MHz):** This range is designated for all types of transmission modes. If local clubs or emergency groups want to set up an independent, autarkic LoRa network separate from the primary MeshCom grid, frequencies within this block are typically selected.
*   **The Standard Tracker Frequency (433.775 MHz):** As noted above, this secondary frequency is already integrated into the standard for offloading pure positioning data via LoRa-APRS.

### 2. The License-Free Alternative (ISM Band)
The frequency block from **433.050 to 434.790 MHz** is shared globally as an **ISM** (Industrial, Scientific, and Medical) band.

*   **No License Required:** Anyone can transmit here without a call sign or an amateur radio license.
*   **Meshtastic instead of MeshCom:** Unlicensed users looking for mesh networking in this band typically use the **Meshtastic** protocol instead of MeshCom.
*   **Strict Regulatory Constraints:** Under ISM rules, maximum transmission power is legally limited to a tiny **10 mW ERP**, and strict duty-cycle limits apply. Licensed amateur radio operators using MeshCom are allowed significantly higher output power (typically 100–500 mW native hardware output on mobile nodes).

### 3. Moving Out of the 433 MHz Noise (The 868 MHz Option)
The 433 MHz ISM band in Europe is heavily congested by consumer devices like car key fobs, weather stations, and garage door openers. If you want to escape this interference completely, the European standard is to switch to the **868 MHz band**.

*   **Note:** MeshCom itself is historically tied to 433 MHz in Austria. Shifting to 868 MHz effectively means migrating away from MeshCom infrastructure into the license-free **Meshtastic** ecosystem, which features a massive, highly dense node network across Austria on 868 MHz.

---

## 🔒 Part 3: LoRa Synchronization Words & Hardware Filtering

The **LoRa Synchronization Word (Sync Word)** is a critical 8-bit hardware filter configured inside register **`0x39` (`RegSyncWord`)** of the Semtech SX1276 transceiver. It functions as a physical RF barrier inside the radio's baseband modem. If an incoming packet's preamble synchronization pattern does not perfectly match the receiver's configured value, the hardware discards the payload instantly on chip-level, preventing the **`RxDone`** flag from firing and keeping the physical interrupt line completely silent.

### Technical Reference Matrix
While register `0x39` accepts any 8-bit value from `0x00` to `0xFF`, specific combinations have established themselves as international operational standards:

| Sync Word | Network Profile | Operational Target / Description |
| :--- | :--- | :--- |
| **`0x2B`** | **Meshtastic Default** | **MeshCom 4.0** (ÖVSV standard for amateur radio mesh infrastructures operating at **433.175 MHz**). Official standard for Meshtastic & MeshCom. Triggers advanced hardware preamble correlation rules in the silicon for maximum noise immunity near the thermal noise floor. |
| **`0x12`** | **Private Network** | **MeshCore Default, LoRa-APRS, Reticulum & Semtech Factory Default**. Standard baseline initialization value utilized for isolated point-to-point (P2P) bridging outside global networks. Also used by **ExpressLRS (ELRS)** (868/915 MHz RC links), **disaster.radio**, and **Amazon Sidewalk** (LoRa layer). |
| **`0x34`** | **Public LoRaWAN** | **LoRaWAN Standard**. Official Semtech value for public LoRaWAN networks (TTN, Helium, ChirpStack), also used by **TinyGS** LoRa Satellites (CubeSats). |
| **`0x24`** | **Tactical Mesh** | **Forks of Meshtastic** (Civilian off-grid digital communications). Used globally on the license-free 433 MHz ISM and 868 MHz bands. Requires modern low-data-rate optimization configurations to eliminate sensitivity degradation on legacy SX127x silicon steps. Closed tactical/team Meshtastic forks (ignores public `0x2B` traffic). |
| **`0xF3`** | **FOSSA Systems** | Used by specific commercial/amateur LoRa satellite constellations (e.g., **FOSSASAT**) to prevent interference from terrestrial LoRa traffic. |
| **`0x55`** | **Isolated Array A** | **Legacy Experimental Value**. Alternating bit pattern (`01010101`). *Deprecated for production:* Mimics the LoRa preamble and is prone to false packet triggers (see warning below). |
| **`0xAA`** | **Isolated Array B** | **Legacy Experimental Value**. Alternating bit pattern (`10101010`). *Deprecated for production:* Mimics the LoRa preamble and is prone to false packet triggers (see warning below). |

> ⚠️ **The Preamble Correlation Trap & Hardware Considerations:**
> * **Avoid Sparse & Alternating Patterns (`0x00`, `0xFF`, `0x55`, `0xAA`):** Custom deployments should strictly avoid sync words with sparse bit transitions (`0x00`, `0xFF`) as well as alternating bit patterns (`0x55` = `01010101`, `0xAA` = `10101010`). Semtech datasheets advise against `0x55` and `0xAA` because they resemble the **LoRa preamble**, which can cause false packet triggers on background noise or mid-preamble, increasing packet error rates. For custom isolated networks, irregular bit patterns (such as `0x14`, `0x42`, or `0x69`) are recommended instead.
> * **SX127x vs. SX126x Mapping:** The 1-byte values listed above apply to the **SX127x** transceiver series. Newer **SX126x** chips use 2-byte sync words internally (for example, `0x12` maps to `0x1424`, and `0x34` maps to `0x3444`). Most modern radio libraries handle this translation automatically.
> * **Coexistence on `0x2B`:** Because both **Meshtastic** and **MeshCom** share the `0x2B` sync word, cross-network interference is primarily avoided through different center frequencies and Spreading Factors (SF).

### MeshCom Co-Existence Layout
To successfully capture live telemetry frames from official ÖVSV MeshCom gateways, the firmware forces a multi-register synchronization sequence during the unified thread-safe constructor pass:

```cpp
// Predefined LoRa Sync Word options for hardware correlation filtering, see docs/lora/device-settings.md
constexpr uint8_t LORA_SYNC_WORD_PUBLIC_NETWORK_MODE     = 0x34; // LoRaWAN Default Gateways, The Things Network (TTN), Helium, ChirpStack,
                                                                 // TinyGS (0x34) LoRa Satellites (Cubesats)
constexpr uint8_t LORA_SYNC_WORD_MESHTASTIC_NETWORK_MODE = 0x2B; // Meshtastic Default
constexpr uint8_t LORA_SYNC_WORD_MESHCOM_NETWORK_MODE    = 0x2B; // MeshCom (ÖVSV standard for amateur radio mesh), also Meshtastic official standard
constexpr uint8_t LORA_SYNC_WORD_TACTICAL_NETWORK_MODE   = 0x24; // Tactical/Team Meshtastic forks (Civilian off-grid tactical mesh network)
constexpr uint8_t LORA_SYNC_WORD_PRIVATE_NETWORK_MODE    = 0x12; // Semtech Default (Factory default / Private point-to-point), LoRa-APRS, MeshCore, Reticulum,
                                                                 // ExpressLRS (ELRS), TinyGS (0x12) LoRa Satellites (Cubesats)
                                                                 // disaster.radio / Ripple (former, specialized off-grid-mesh projects)
constexpr uint8_t LORA_SYNC_WORD_FOSSASAT_NETWORK_MODE   = 0xF3; // Special commercial/amateur LoRa satellite constellations (e.g., FOSSASAT) to prevent interference from terrestrial LoRa traffic
constexpr uint8_t LORA_SYNC_WORD_ISOLATED_ARRAY_A        = 0x55; // Legacy / Experimental (Deprecated: prone to false preamble triggers, prefer irregular bytes like 0x14)
constexpr uint8_t LORA_SYNC_WORD_ISOLATED_ARRAY_B        = 0xAA; // Legacy / Experimental (Deprecated: prone to false preamble triggers, prefer irregular bytes like 0x42)

// Active hardware synchronization configuration selector
constexpr uint8_t LORA_SYNC_WORD = LORA_SYNC_WORD_MESHCOM_NETWORK_MODE;
```

This variable updates the Semtech baseband modem alongside an expanded preamble length of **12 symbols** to prevent packet dropping during long correlation windows:

```cpp
// Write the dynamic sync word byte to register 0x39
writeRegister(0x39, Config::LORA_SYNC_WORD);

// Expand hardware preamble length to 12 symbols for robust network correlation
writeRegister(0x20, 0x00U); // RegPreambleMsb
writeRegister(0x21, 0x0CU); // RegPreambleLsb (12 = 0x0C)
```

---

## ⚙️ Part 4: SX126x Architecture & Register/Opcode Mapping

While legacy boards (such as LilyGO T-Beam v1.1 or TTGO LoRa32) rely on the register-mapped **SX127x** series, newer hardware platforms (such as Heltec WiFi LoRa 32 V3 or LilyGO T-Beam Supreme) utilize the **SX126x** transceiver family (SX1262 / SX1268).

Unlike the SX127x, the SX126x is **command-driven (Opcode API)** with an internal state machine. Direct SPI register reads/writes are largely replaced by SPI opcodes, and the host MCU must monitor the hardware **`BUSY`** pin before issuing any command.

### SX127x vs. SX126x Hardware Mapping

| Feature / Function | SX127x (Register-Based) | SX126x (Opcode / Extended Register) |
| :--- | :--- | :--- |
| **Sync Word Storage** | Register `0x39` (**1 Byte**) | Registers `0x0740` (MSB) & `0x0741` (LSB) (**2 Bytes**) |
| **Modulation Config** | Registers `0x1D`, `0x1E`, `0x26` | Opcode `0x8B` (`SetModulationParams`) |
| **Packet & Preamble** | Registers `0x20` (MSB) & `0x21` (LSB) | Opcode `0x8C` (`SetPacketParams`, Bytes 1–2 = `0x000C`) |
| **RF Frequency** | Registers `0x06`, `0x07`, `0x08` | Opcode `0x86` (`SetRfFrequency`) |
| **IRQ Status & Clear** | Register `0x12` (`RegIrqFlags`) | Opcode `0x12` (`GetIrqStatus`) & `0x02` (`ClearIrqStatus`) |
| **Chip Identification** | Register `0x42` (`RegVersion` == `0x12`) | No version register; read default sync word at `0x0740` (`0x14`) |
| **TCXO & RF Switch** | External / GPIO controlled | Opcodes `0x97` (`SetDIO3AsTcxoCtrl`) & `0x9D` (`SetDIO2AsRfSwitchCtrl`) |

### 1-Byte to 2-Byte Sync Word Translation
Because the SX126x baseband correlator expects a 16-bit sync word, 1-byte SX127x sync words are mapped by splitting the upper and lower nibbles and padding them with `4` (`0x4`):

$$\text{SX126x SyncWord} = ((\text{Byte}_{8\text{bit}} \ \& \ \text{0xF0}) \ll 8) \ | \ ((\text{Byte}_{8\text{bit}} \ \& \ \text{0x0F}) \ll 4) \ | \ \text{0x0404}$$

| Network Profile | SX127x (1-Byte) | SX126x (2-Byte) | Register `0x0740` / `0x0741` |
| :--- | :--- | :--- | :--- |
| **MeshCom / Meshtastic** | `0x2B` | **`0x24B4`** | `0x24`, `0xB4` |
| **Private / LoRa-APRS** | `0x12` | **`0x1424`** | `0x14`, `0x24` |
| **Public LoRaWAN** | `0x34` | **`0x3444`** | `0x34`, `0x44` |
| **Tactical Mesh** | `0x24` | **`0x2444`** | `0x24`, `0x44` |
| **FOSSA Systems** | `0xF3` | **`0xF434`** | `0xF4`, `0x34` |

### SX126x Initialization Sequence (C++ / SPI)
Unlike the SX127x, writing the sync word on an SX126x requires the `WriteRegister` opcode (`0x0D`) followed by a 16-bit address, while ensuring the `BUSY` line is low:

```cpp
// Convert 1-byte SX127x sync word to 2-byte SX126x format
constexpr uint16_t mapSyncWordToSx126x(uint8_t syncWord8) {
    return ((static_cast<uint16_t>(syncWord8 & 0xF0) << 8) |
            (static_cast<uint16_t>(syncWord8 & 0x0F) << 4) | 0x0404U);
}

// Write 2-byte sync word to SX126x registers 0x0740 and 0x0741 via Opcode 0x0D
void setSx126xSyncWord(uint8_t syncWord8) {
    uint16_t sw16 = mapSyncWordToSx126x(syncWord8);
    uint8_t payload[5] = {
        0x0D,                                      // Opcode: WriteRegister
        0x07, 0x40,                                // 16-bit Start Address (0x0740)
        static_cast<uint8_t>((sw16 >> 8) & 0xFF),  // MSB -> 0x0740
        static_cast<uint8_t>(sw16 & 0xFF)          // LSB -> 0x0741
    };
    waitWhileBusy();                               // Mandatory SX126x BUSY pin check
    spiTransfer(payload, sizeof(payload));
}
```

> ⚠️ **Critical SX126x Pitfall (Silent Receiver / TCXO):**
> Most SX1262 modules (e.g., Heltec V3, Ra-01SH) use a **TCXO** powered via `DIO3` and an internal antenna switch driven by `DIO2`. If you do not explicitly send **`SetDIO3AsTcxoCtrl` (`0x97`)** and **`SetDIO2AsRfSwitchCtrl` (`0x9D`)** during initialization, the chip will fail to lock its PLL (`XOSC_START_ERR`) and remain completely deaf.


## 🛠️ Part 5: Troubleshooting Packet Drops & Telemetry Failures

When listening to live MeshCom grids or experimental point-to-point networks, your terminal might remain silent despite active RF activity in your area. This section details how to diagnose, interpret, and resolve low-level hardware anomalies using the automated register metrics layer.

### 1. The Low-Level Interrupt Diagnostics (Bypassing FreeRTOS Flags)
If your application task does not wake up, you must verify if the Semtech silicon is tracking any signals at all. By reading register **`0x12` (`RegIrqFlags`)**, you can isolate the failure source instantly:

```text
  Reg 0x12 Value    Hardware Status          Root Cause & Resolution
  --------------    -----------------------  -------------------------------------------------------------
       0x00         No Signal Detected       - Transceiver is out of range or frequency calibration mismatch.
                                             - Check if your device is actively in RXCONTINUOUS mode [0x85].
       0x20         PayloadCrcError Active   - The packet was received but failed the hardware checksum.
                                             - Caused by bad SNR (noise floor interference) or SF mismatch.
       0x80         RxTimeout Active         - The preamble was detected, but the complete frame timed out.
                                             - Increase your preamble length to 12 symbols (Reg 0x20/0x21).
       0x40         RxDone Success           - The hardware captured, matched, and validated the packet!
                                             - If terminal stays silent, check your FreeRTOS semaphore handles.
```

---

### 2. Common Integration Failure Modes

#### Symptom A: Zero Output from `readRegister(0x42)` during Boot
* **Root Cause:** The SPI peripheral host is misconfigured, or the hardware reset pin failed to pull low during the unified constructor pass.
* **Resolution:** Verify that `spi_bus_initialize()` is bound to the correct Host controller (`SPI2_HOST` for standard LilyGO TTGO boards). Ensure your initialization logic executes safely on **Core 0** inside the main task context to guarantee uniform driver registration.

#### Symptom B: The First Packet Arrives, then Total Silence
* **Root Cause:** The hardware interrupt flags within register `0x12` were not acknowledged, or the internal read pointer (`RegFifoAddrPtr`) was not explicitly reset to the packet's current base address.
* **Resolution:** You must overwrite register `0x12` with its active bit mask at the **absolute beginning** of your processing chain. This forces the physical `DIO0` line to fall back to logic low, enabling the ESP32 to detect subsequent rising edges (`GPIO_INTR_POSEDGE`). Always call `setModeReceiver()` at the end of the extraction loop to clear baseband modulations.

#### Symptom C: Constant `PayloadCrcError` [0x20] on Local MeshCom Traffic
* **Root Cause:** Inverted IQ configurations or local industrial noise (shared 433 MHz ISM congestion) is corrupting incoming headers.
* **Resolution:** Verify that your local modem target bandwidth exactly mirrors the MeshCom standard of **250 kHz** and **SF11**. For extreme near-field interference, ensure your `calculateTelemetry()` float calculation is active to track if sub-noise SNR regressions are pulling the signal strength below `-120 dBm`.
