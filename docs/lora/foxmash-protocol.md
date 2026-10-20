# FoxMash - Protocol

English version. In case of doubt, the English text shall be binding.
*[Deutsche Version anzeigen](foxmash-protocol_de.md)*

## Automatic Stations at the ARDF Transmitting Facilities (Foxes)

A fox regularly broadcasts its identifier and telemetry data.
This is done, whenever possible, at times when the fox is not transmitting on the 80m or 2m bands.
The purpose of this is, on the one hand, the time synchronization of adjacent nodes, and on the other hand, the regular official announcement of the call sign.
Usually, the call sign of the organizer or host of the direction-finding competition is used.

## Beacon

Composed based on the modified AX.25, APRS, and MeshCom protocols as follows:

Pseudo-Byte-Stream:
`{OE3TKT-03nnnnnn`


### Structure:

* **Byte 0: `{` (0x7B)**
  * **Data Type Identifier (DTI):** Derived from the APRS & MeshCom protocol specification (see [APRS Protocol Reference](https://ui-view.net)).
  * **Reasoning:** This data type identifier is currently marked as `[Unused]` in the specification, and the character visually resembles a station icon.

* **Bytes 1–6: `"OE3TKT"` (0x4F 0x45 0x33 0x54 0x4B 0x54)**
  * **International Call Sign:** Formatted as a 1–2 character prefix, followed by one or more digits, and a 1–3 letter suffix.
  * **Padding:** This field is strictly 6 characters wide. If the call sign is shorter, it must be right-padded with trailing spaces.

* **Bytes 7+: Station Number / SSID Identifier**
  * **Standard Notation (IDs 0–99):** Uses a hyphen followed by a two-digit SSID (Bytes 7–9), for example, `"-03"` (0x2D 0x30 0x33).
  * **Extended Notation (IDs ≥ 100):** If the station number exceeds 100, you may take any letters as described in https://aprs11/SSIDs.txt (e.g., `OE3TKT-S6` for any special 6th station).
  * **Backup / Hot-Standby Nodes:** To designate a station as a replacement node or an immediate hot-standby node, the hyphen is replaced by a tilde symbol (e.g., `OE3TKT~03`).

* **Bytes 10–13: Telemetry Data (`nnnnnn` / Timestamp)**
  * **Timestamp:** Signed 32-bit integer representing the number of milliseconds since the start of the competition day.

* **Byte 14: Own Battery Level**
  * **Value:** Unsigned 8-bit integer (`unsigned char`) representing the node's own battery capacity in percent (0 to 100%).
      A value of `100` (0x64) means fully charged, while `0` means empty.
      Values above 100 can be used to indicate charging or mains power (e.g., `255`).

* **Bytes 15+: Heard Neighbor Nodes (Format: `xRSqxRSqxRSq...`)**
  * Each neighbor node found in the mesh is represented by a multi-byte block containing its SSID, link quality, and battery status.
  * **Bytes 15–16 (x):** Neighbor SSID (16-bit unsigned integer, e.g., `0x0003` for station number 3).
  * **Byte 17 (R):** RSSI value + 164 stored as an `unsigned char` (e.g., `0x3A` = 58 ➡️ 58 - 164 = -106 dBm).
  * **Byte 18 (S):** SNR value multiplied by 4 stored as a `signed char` (e.g., `0x42` = 66 ➡️ 66 / 4 = +16.5 dB).
  * **Byte 19 (q):** Combined Quality and Battery Status Byte:
    * **Bits 7–6 (Battery Status of the neighbor):**
      * `00` = Normal (Battery OK / Powered)
      * `01` = Low (Battery low, intervention recommended)
      * `10` = Critical (Battery critical, node might shut down soon)
      * `11` = Reserved
    * **Bits 5–0 (Counter):** Moving average counter of how many times this station was heard within the last 30 minutes (Max value: 63). If this counter is ≥ 3, the neighbor is considered a stable link. Stations not heard at least once within 30 minutes are dropped from the list.

## Checkpoint Messages (Posten)

As soon as a participant checks in at a checkpoint using their RFID tag or identifier, they are registered at that node. The runner is assigned a timestamp and their ID (runner number). The mapping between the runner number and the RFID code is stored within the system. During the next available time slot, a checkpoint message is transmitted to the destination with QoS 2 (guaranteed packet delivery, duplicate detection, and filtering).

Messages are sent to the destination and optionally to the start node simultaneously. Both nodes must have an SSID < 15, as the remaining bits in the address byte are reserved for protocol flags.

This message packet is designed to be extremely short and efficient. It contains the node number, runner number, capture timestamp, and the current hop count.

Pseudo-Byte-Stream:
`vppMLzzzzRR`

### Structure (Checkpoint Message):

* **Byte 0: `(` (0x40)**
  * **Data Type Identifier (DTI):** Special identifier designated for FoxMash checkpoint messages.

* **Bytes 1–2: Own SSID (`pp`)**
  * **Value:** 16-bit unsigned integer (`uint16_t`) of the transmitting checkpoint node.

* **Byte 3: `M` (Target SSID, Hop Count & Flags)**
  * **Bits 7–5 (Hop Count):** Dynamic routing counter. If the node sees the destination directly with good quality, this value is `0`. If no acknowledgment is received within 5 minutes, the permitted hop value is incremented by one. If the hop value exceeds the defined maximum, the packet is discarded.
  * **Bit 4 (ACK Flag):** `0` for a standard data message, `1` for an acknowledgment packet (ACK).
  * **Bits 3–0 (Target SSID):** The SSID of the destination node (Supports IDs from 0 to 15).

* **Bytes 4–5: `L` (Runner Number)**
  * **Value:** 16-bit unsigned integer (`uint16_t`) for unique participant identification (Supports IDs from 0 to 65,535).

* **Bytes 6–9: `zzzz` (Timestamp)**
  * **Value:** Signed 32-bit integer (milliseconds since the start of the day) capturing the exact time the runner checked in at the post. Also used to verify packet validity within the allowed time window.

* **Bytes 10+: `RR` (Routing List / Optional)**
  * **Value:** Optional list of intermediate nodes. For example, when utilizing three relay nodes, this field is structured as `RR RR RR`. It remains empty if a direct connection is established.

### Acknowledgments and Retries
The maximum number of retries is configurable (default: 4). Every checkpoint packet must be acknowledged. The acknowledgment uses the exact same format (`vppMLzzzzRR`), but with **Bit 4 in Byte 3 (`M`) set to `1`**. The return routing follows the identical path in the reverse direction based on the route recorded in the packet (source routing). Remit nodes detect and discard duplicate packets by matching the runner's unique timestamp.
