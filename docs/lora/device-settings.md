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
