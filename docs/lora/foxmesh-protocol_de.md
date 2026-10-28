# FoxMash - Protokoll

Deutsche Übersetzung. Im Zweifelsfall ist die englischsprachige Version bindend.
*[View English Version](foxmash-protocol.md)*

## Automatische Stationen bei den ARDF-Sendeeinrichtungen (Füchsen)

Ein Fuchs sendet regelmäßig seine Kennung (Identifier) und Telemetriedaten aus.
Dies geschieht, wann immer möglich, zu Zeiten, in denen kein Sendebetrieb des Fuchses im 80m- oder 2m-Band stattfindet.
Dies dient einerseits der Zeitsynchronisation der Nachbarknoten und andererseits der regelmäßigen, behördlich vorgeschriebenen Aussendung des Rufzeichens.
Üblicherweise wird das Rufzeichen des Ausrichters bzw. Veranstalters des Peilwettbewerbs verwendet.

## Bake (Broadcast)

Setzt sich wie folgt aus den modifizierten AX.25-, APRS- und MeshCom-Protokollen zusammen:

Pseudo-Byte-Stream:
`{OE3TKT-03nnnnxxRSqxxRSqxxRSq`

### Struktur:

* **Byte 0: `{` (0x7B)**
  * **Data Type Identifier (DTI):** Abgeleitet aus der APRS- & MeshCom-Protokollspezifikation (siehe [APRS Protocol Reference](https://ui-view.net)).
  * **Begründung:** Dieser Datentyp-Bezeichner ist in der Spezifikation derzeit noch als `[Unused]` (unbenutzt) markiert, und das Zeichen sieht visuell wie ein Stationssymbol aus.

* **Bytes 1–6: `"OE3TKT"` (0x4F 0x45 0x33 0x54 0x4B 0x54)**
  * **Internationales Rufzeichen:** Formatiert als 1–2 Zeichen langes Präfix, gefolgt von einer oder mehreren Ziffern und einem 1–3 Zeichen langen Suffix.
  * **Padding (Auffüllung):** Dieses Feld ist strikt 6 Zeichen lang. Wenn das Rufzeichen kürzer ist, muss es rechtsbündig mit Leerzeichen aufgefüllt werden.

* **Bytes 7+: Stationsnummer / SSID-Kennung**
  * **Standard-Notation (IDs 0–99):** Verwendet einen Bindestrich, gefolgt von einer zweistelligen SSID (Bytes 7–9), zum Beispiel `"-03"` (0x2D 0x30 0x33).
  * **Erweiterte Notation (IDs ≥ 100):** Wenn die Stationsnummer 100 überschreitet, können beliebige Buchstaben verwendet werden, wie unter https://aprs11/SSIDs.txt beschrieben (z. B. `OE3TKT-S6` für eine beliebige spezielle 6. Station).
  * **Ersatz- / Hot-Standby-Knoten:** Um eine Station als Ersatzstation oder sofort einsatzbereiten Knoten zu kennzeichnen, wird der Bindestrich durch ein Tilde-Zeichen ersetzt (z. B. `OE3TKT~03`).

* **Bytes 10–13: Telemetriedaten (`nnnnnn` / Zeitstempel)**
  * **Zeitstempel:** Vorzeichenbehaftete 32-Bit-Ganzzahl (Signed 32-Bit Integer), die die Millisekunden seit Beginn des Wettbewerbstages darstellt.
  Dieser Zeitstempel dient gleichzeitig auch als Nachrichten-ID, und die gesamte Nachricht wird gehasht und damit kann festgestellt werden, ob sie doppelt ist.

* **Byte 14: Eigener Batteriestand**
  * **Wert:** Vorzeichenlose 8-Bit-Ganzzahl (`unsigned char`), die die eigene Batteriekapazität in Prozent (0 bis 100 %) darstellt. Ein Wert von `100` (0x64) bedeutet voll geladen, `0` bedeutet leer. Werte über 100 können optional für Netzbetrieb oder Ladevorgang reserviert werden (z. B. `255`).

* **Bytes 15+: Gehörte Nachbarknoten (Format: `xxRSqxxRSqxxRSq...`)**
  * Jeder im Mesh gefundene Nachbarknoten wird durch einen Multi-Byte-Block dargestellt, der seine SSID, die Link-Qualität und den Batteriestatus enthält.
  * **Bytes 15–16 (x):** Nachbar-SSID (16-Bit Unsigned Integer, z. B. `0x0003` für Stationsnummer 3).
  * **Byte 17 (R):** RSSI-Wert + 164 als `unsigned char` (z. B. `0x3A` = 58 ➡️ 58 - 164 = -106 dBm).
  * **Byte 18 (S):** Vierfacher SNR-Wert als `signed char` (z. B. `0x42` = 66 ➡️ 66 / 4 = +16.5 dB).
  * **Byte 19 (q):** Kombiniertes Qualitäts- und Batteriestatus-Byte:
    * **Bits 7–6 (Batteriezustand des Nachbarn):**
      * `00` = Normal (Batterie OK / Netzbetrieb)
      * `01` = Niedrig (Batterie schwach, Wartung empfohlen)
      * `10` = Kritisch (Batterie kritisch, Knoten fällt bald aus)
      * `11` = Reserviert
    * **Bits 5–0 (Zähler):** Gleitender Zähler, wie oft diese Station in den letzten 30 Minuten gehört wurde (Maximalwert: 63). Wenn dieser Zähler ≥ 3 ist, gilt der Nachbar als stabiler Link. Stationen, die innerhalb von 30 Minuten kein einziges Mal gehört wurden, werden aus der Liste gelöscht.

Falls der eigne Knoten die Ziel/Start-Knoten nicht selbst sehen, so müssen

## Nachrichten von Posten

Sobald eine Person am Postenschirm seine Kennung bekannt gibt wird er an diesem Knoten registriert.
Ein Zeitstempel und seine ID wird zugewiesen (Läufernummer).
Die Läufernummer und die entsprechende RFID-Nummer ist im System hinterlegt und es wird beim nächsten Zeitschlitz eine Posten-Nachricht an das Ziel verschickt.

Das Ziel und der Start-Knoten ist dem System bekannt (muss nicht zwingend SSID 0 haben).
Jedoch: Muss eine SSID < 15 haben, da wir die anderen bits für Flags verwenden.
Eventuell wird an beide Knoten, Ziel und Start gleichzeitig verschickt.
Die QoS ist dabei nicht mehr 0 sondern 2. (Sicherstellung der Paketzustellung und doppelte Pakete werden erkannt und weggeworfen).

Dieses Nachrichtenpaket ist sehr kurz und effizient aufgebaut:

Wir brauchen folgende Informationen: Knotennummer, Läufernummer, Zeitstempel, und der wievielte Hopp es ist.

## Nachrichten von Posten

Sobald sich ein Teilnehmer am Postenschirm per RFID/Kennung anmeldet, wird er an diesem Knoten registriert. Dem Läufer wird ein Zeitstempel und seine ID (Läufernummer) zugewiesen. Die Verknüpfung zwischen Läufernummer und RFID-Code ist im System hinterlegt. Beim nächsten verfügbaren Zeitschlitz wird eine Posten-Nachricht mit der Dienstgüte QoS 2 (Sicherstellung der Paketzustellung, Erkennung und Verwerfen von Duplikaten) an das Ziel übermittelt.

Die Nachrichten werden an das Ziel und optional gleichzeitig an den Start-Knoten gesendet. Beide Knoten müssen eine SSID < 15 besitzen, da die verbleibenden Bits im Adress-Byte für Protokoll-Flags reserviert sind.

Dieses Nachrichtenpaket ist extrem kurz und effizient aufgebaut. Es enthält die Knotennummer, die Läufernummer, den Zeitstempel der Erfassung sowie den aktuellen Hop-Zähler.

Pseudo-Byte-Stream:
`vppMLzzzzRR`

### Struktur (Posten-Nachricht):

* **Byte 0: `(` (0x40)**
  * **Data Type Identifier (DTI):** Spezieller Bezeichner für FoxMash-Postenmeldungen.

* **Bytes 1–2: Eigene SSID (`pp`)**
  * **Wert:** 16-Bit vorzeichenlose Ganzzahl (`uint16_t`) des sendenden Posten-Knotens.

* **Byte 3: `M` (Ziel-SSID, Hop-Limit / TTL & Flags)**
  * **Bits 7–5 (Hop-Limit / TTL):** Startwert für die maximale Anzahl an erlaubten Hops (Time-to-Live). Der Startknoten setzt diesen Wert (z. B. auf 3). Jeder weiterleitende Zwischenknoten verringert (dekrementiert) diesen Wert um 1. Erreicht der Wert `0`, wird das Paket nicht mehr weitergeleitet und verworfen.
  * **Bit 4 (ACK-Flag):** `0` für eine normale Nachricht, `1` für ein Bestätigungspaket (ACK).
  * **Bits 3–0 (Ziel-SSID):** Die SSID des Zielknotens (Unterstützt IDs von 0 bis 15).

* **Bytes 4–5: `L` (Läufernummer)**
  * **Wert:** 16-Bit vorzeichenlose Ganzzahl (`uint16_t`) zur eindeutigen Identifikation des Teilnehmers (Unterstützt IDs von 0 bis 65.535).

* **Bytes 6–9: `zzzz` (Zeitstempel)**
  * **Wert:** Vorzeichenbehaftete 32-Bit-Ganzzahl (Millisekunden seit Tagesbeginn), die den exakten Zeitpunkt der Registrierung des Läufers am Posten festhält. Dient auch zur Gültigkeitsprüfung im Zeitrahmen.

* **Bytes 10+: `RR` (Routingliste / Optional)**
  * **Wert:** Optionale Liste der Zwischenknoten. Bei der Verwendung von beispielsweise drei Relaisknoten setzt sich das Feld aus `RR RR RR` zusammen. Bleibt leer, wenn eine Direktverbindung besteht.

### Bestätigung und Wiederholungen (Retries)
Die maximale Anzahl an Sendeversuchen (Retries) ist konfigurierbar (Standard: 4). Jedes Posten-Paket muss zwingend bestätigt werden. Die Bestätigung verwendet das exakt gleiche Format (`vppMLzzzzRR`), wobei jedoch **Bit 4 im Byte 3 (`M`) auf `1` gesetzt** ist. Das Antwort-Routing nutzt anhand des im Paket aufgezeichneten Pfades dieselbe Strecke in umgekehrter Richtung (Source Routing). Duplikate werden empfängerseitig anhand des eindeutigen Läufer-Zeitstempels erkannt und verworfen.
