# Protocol MeshCom 4.0

## Stream - Example
```
:~1~9~SP3TUT-1,OE1XAR-12,OE3TKT-12>*:z okolic Pi~~y kto~~ tu jest?~~~~~(~a~
                     S
Hex: 3A BB 31 F3 39 93 53 50 33 54 55 54 2D 31 2C 4F 45 31 58 41 52 2D 31 32 2C 4F 45 33 54 4B 54 2D 31 32 3E 2A 3A 7A 20 6F 6B 6F 6C 69 63 20 50 69 C5 82 79 20 6B 74 6F C5 9B 20 74 75 20 6A 65 73 74 3F 00 03 88 16 C5 28 8C 61 7E
     |  ===========  | ===================================================================================
     |       |       |   |
     |       |       |    > APRS Address Header (Source,Path, Destination) - "SP3TUT-1": Source Callsign of the transmitting amateur radio station from Poland, using SSID 1.
     |       |       |    >  -
     |       |       |    >
     |       |       > Flags & Hops [7]
B02  |       > Message Identifier "~1~9" 0xBB31F339 - The unique 32-bit Message ID transmitted in Little-Endian format (least significant byte first).
     |                                                It is used by the mesh network to track packets and prevent duplication loops.
B01  > APRS Data Type Identifier ":" 0x3A - APRS Text Message, used for person-to-person Direct Messages (DMs), group messages, or general announcements.
                                                 Following this character, the parser expects a destination callsign, a tracking number, and the message content.
                                 "@" 0x40 - APRS Position with Timestamp, used for automated position reports that include a precise time (such as weather node beacons or sttic trackers).
                                                 It tells the terminal to parse fixed-length chunks for latitude, longitude, and elevation.
                                 "!" 0x21 - APRS Position without Timestamp, used for real-time mobile tracking (e.g., a node) with an integrated GPS sending live movement updates).
                                                 It skips the time fields to reduce airtime over LoRa.
                                 ";" 0x3B - APRS Object Report, used to map an "Object" rather than a station.
                                                 For example, a node can broadcast the location of a local hamfest, emergency shelter, or repeater.
                                                 It is followed by a fixed 9-character object name.
                                 ">" 0x3E - APRS Status Report: Used to transmit a station's current operating state or hardware information rather than geographic coordinates.
                                 "$" 0x24 - APRS Raw NMEA Sentence: A legacy data type that allows raw GPS string passthrough (like $GPRMC or $GPGGA) directly from a serial GPS mouse.
Other DTI(Data Type Identifiers)
 "=" 0x3D - Position with compressed APRS capability, a standard position beacon, but formatted in Base-91 compression to save airtime over busy RF networks.
 "/" 0x2F - Mic-E Format Data, Microphone Encoder telemetry. Used by commercial radios (like Yaesu or Kenwood) to embed a GPS burst into a standard voice transmission PTT release.
 "`" 0x60 - Mic-E Old Format, Legacy Microphone Encoder burst, rarely used by modern trackers but still supported for backward compatibility.
 "_" 0x5F - Positionless Weather Report, used by automated weather stations (like fine-offset units or CWOP stations) to transmit pure wind speed, temperature, and barometric values without re-sending latitude/longitude.
 ")" 0x29 - Item Report, identical to a map Object (;), but used for temporary or small Points of Interest (POIs) with a much shorter name (1–9 characters) and no timestamp.
 "<" 0x3C - Station Capabilities, sent by intelligent digipeaters and internet gateways (iGates) to report what features they support (e.g., whether they handle messaging or special path routing).
 "T" 0x54 - Raw Telemetry Data, used for automated analog data streams. Frequently found on high-altitude balloon payloads or weather satellites transmitting five channels of changing mathematical numbers.
 "S" 0x53 - Telemetry Structural Definition, defines the scale and offsets of raw numbers sent in the T packet (so client maps like aprs.fi know if a raw value of 142 means volts, millibars, or degrees).
 "?" 0x3F - Query/Poll Command, a packet explicitly asking another station or a server to respond. For instance, sending ?APRSP asks surrounding nodes for their current paths.
 "{" 0x7B - User-Defined / Proprietary, reserved entirely for experimental protocols or internal applications. MeshCom and similar experimental LoRa platforms utilize these spaces to run custom network setups.
```
## Callsign
```
Every callsign is build with [Personal Amateur Radio Callsign]-[SSID], SSID is a number between 1 and 99.
• -0 (or no suffix): Standard home station / fixed station (usually avoided by MeshCom for mobile operation).
• -1 to -3: Additional stations with potential special functions (e.g., digipeaters).
• -7: Portable stations, handheld radios (e.g., a Heltec or T-Echo node carried in a pocket).
• -9: Mobile stations (vehicles).
• -10: Gateways, digipeaters, or Winlink nodes.
• -11: Pure client stations without gateway functionality.
• -12: Gateway stations including mesh routing functionality (very common for permanently running master nodes with an internet/Wi-Fi connection).
• -13: Weather stations or telemetry sensors.
```
