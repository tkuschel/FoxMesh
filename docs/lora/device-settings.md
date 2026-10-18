# LoRa Mesh & Device Settings (Austria / EU)

This document provides a comprehensive overview of the **MeshCom** parameters, frequency structures, and alternative LoRa options according to the official Austrian (**ÖVSV**) 70cm band plan.

---

## 📻 Part 1: MeshCom Frequency & Required Parameters

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
| **`0x34`** | **Public LoRaWAN** | **MeshCom Standard** (ÖVSV standard for amateur radio mesh infrastructures operating at **433.175 MHz**). Triggers advanced hardware preamble correlation rules in the silicon for maximum noise immunity near the thermal noise floor. |
| **`0x24`** | **Tactical Mesh** | **Meshtastic Default** (Civilian off-grid digital communications). Used globally on the license-free 433 MHz ISM and 868 MHz bands. Requires modern low-data-rate optimization configurations to eliminate sensitivity degradation on legacy SX127x silicon steps. |
| **`0x12`** | **Private Network** | **Semtech Factory Default**. Standard baseline initialization value utilized for isolated point-to-point (P2P) bridging outside global networks. |
| **`0x55`** | **Isolated Array A** | **Custom / Experimental**. Alternating bit pattern (`01010101`) providing optimal transition density for the internal clock-data recovery (CDR) PLL loop. |
| **`0xAA`** | **Isolated Array B** | **Custom / Experimental**. Alternating bit pattern (`10101010`) offering optimal signal integrity transitions for proprietary cryptography deployments. |

> ⚠️ **The Preamble Correlation Trap:**
> Custom deployments should strictly avoid sync words with sparse bit transitions (such as `0x00` or `0xFF`). The internal baseband correlation engine relies on distinct signal transitions to sync its timing grid. Continuous sequences of zeros or ones drastically degrade receiver sensitivity.

### MeshCom Co-Existence Layout
To successfully capture live telemetry frames from official ÖVSV MeshCom gateways, the firmware forces a multi-register synchronization sequence during the unified thread-safe constructor pass:

```cpp
// Predefined LoRa Sync Word options for hardware correlation filtering
constexpr uint8_t LORA_SYNC_WORD_PUBLIC_NETWORK_MODE       = 0x34; // MeshCom (ÖVSV standard for amateur radio mesh)
constexpr uint8_t LORA_SYNC_WORD_TACTICAL_NETWORK_MODE     = 0x24; // Meshtastic (Civilian off-grid tactical mesh network)
constexpr uint8_t LORA_SYNC_WORD_PRIVATE_NETWORK_MODE      = 0x12; // Semtech Default (Factory default / Private point-to-point)
constexpr uint8_t LORA_SYNC_WORD_CUSTOM_NETWORK_MODE       = 0x55; // Custom / Experimental individual networks (highly isolated)
constexpr uint8_t LORA_SYNC_WORD_EXPERIMENTAL_NETWORK_MODE = 0xAA; // Custom / Experimental individual networks (highly isolated)

// Active hardware synchronization configuration selector
constexpr uint8_t LORA_SYNC_WORD = LORA_SYNC_WORD_PUBLIC_NETWORK_MODE;
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

## 🛠️ Part 4: Troubleshooting Packet Drops & Telemetry Failures

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
