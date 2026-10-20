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

* **Byte 0: `"{ "` (0x7B)**
  * **Data Type Identifier (DTI):** Derived from the APRS & MeshCom protocol specification (see [APRS Protocol Reference](https://ui-view.net)).
  * **Reasoning:** This data type identifier is currently marked as `[Unused]` in the specification, and the character visually resembles a station icon.

* **Bytes 1–6: `"OE3TKT"` (0x4F 0x45 0x33 0x54 0x4B 0x54)**
  * **International Call Sign:** Formatted as a 1–2 character prefix, followed by one or more digits, and a 1–3 letter suffix.
  * **Padding:** This field is strictly 6 characters wide. If the call sign is shorter, it must be right-padded with trailing spaces.

* **Bytes 7+: Station Number / SSID Identifier**
  * **Standard Notation (IDs 0–99):** Uses a hyphen followed by a two-digit SSID (Bytes 7–9), for example, `"-03"` (0x2D 0x30 0x33).
  * **Extended Notation (IDs ≥ 100):** If the station number exceeds 100, you may take any letters as described in https://aprs11/SSIDs.txt (e.g., `OE3TKT-S6` for any special 6th station).
  * **Backup / Hot-Standby Nodes:** To designate a station as a replacement node or an immediate hot-standby node, the hyphen is replaced by a tilde symbol (e.g., `OE3TKT~03`).

* **Bytes 10–13: Telemetry Data (`nnnnnn`)**
  * **Timestamp:** Signed 32-bit integer representing the number of milliseconds since the start of the competition day.
