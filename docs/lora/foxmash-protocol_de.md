# FoxMash - Protokoll

Deutsche Übersetzung. Im Zweifelsfall ist die englischsprachige Version bindend.
*[View English Version](foxmash-protocol.md)*

## Automatische Stationen bei den ARDF-Sendeeinrichtungen (Füchsen)

Ein Fuchs sendet regelmäßig seine Kennung (Identifier) und Telemetriedaten aus.
Dies geschieht, wann immer möglich, zu Zeiten, in denen kein Sendebetrieb des Fuchses im 80m- oder 2m-Band stattfindet.
Dies dient einerseits der Zeitsynchronisation der Nachbarknoten und andererseits der regelmäßigen, behördlich vorgeschriebenen Aussendung des Rufzeichens.
Üblicherweise wird das Rufzeichen des Ausrichters bzw. Veranstalters des Peilwettbewerbs verwendet.

## Bake

Setzt sich wie folgt aus den modifizierten AX.25-, APRS- und MeshCom-Protokollen zusammen:

Pseudo-Byte-Stream:
`{OE3TKT-03nnnnxxRSqyyRSqzzRSq`


### Struktur:

* **Byte 0: `"{ "` (0x7B)**
  * **Data Type Identifier (DTI):** Abgeleitet aus der APRS- & MeshCom-Protokollspezifikation (siehe [APRS Protocol Reference](https://ui-view.net)).
  * **Begründung:** Dieser Datentyp-Bezeichner ist in der Spezifikation derzeit noch als `[Unused]` (unbenutzt) markiert, und das Zeichen sieht visuell wie ein Stationssymbol aus.

* **Bytes 1–6: `"OE3TKT"` (0x4F 0x45 0x33 0x54 0x4B 0x54)**
  * **Internationales Rufzeichen:** Formatiert als 1–2 Zeichen langes Präfix, gefolgt von einer oder mehreren Ziffern und einem 1–3 Zeichen langen Suffix.
  * **Padding (Auffüllung):** Dieses Feld ist strikt 6 Zeichen lang. Wenn das Rufzeichen kürzer ist, muss es rechtsbündig mit Leerzeichen aufgefüllt werden.

* **Bytes 7+: Stationsnummer / SSID-Kennung**
  * **Standard-Notation (IDs 0–99):** Verwendet einen Bindestrich, gefolgt von einer zweistelligen SSID (Bytes 7–9), zum Beispiel `"-03"` (0x2D 0x30 0x33).
  * **Erweiterte Notation (IDs ≥ 100):** Wenn die Stationsnummer 100 überschreitet, können beliebige Buchstaben verwendet werden, wie unter https://aprs11/SSIDs.txt beschrieben (z. B. `OE3TKT-S6` für eine beliebige spezielle 6. Station).
  * **Ersatz- / Hot-Standby-Knoten:** Um eine Station als Ersatzstation oder sofort einsatzbereiten Knoten zu kennzeichnen, wird der Bindestrich durch ein Tilde-Zeichen ersetzt (z. B. `OE3TKT~03`).

* **Bytes 10–13: Telemetriedaten (`nnnnnn`)**
  * **Zeitstempel:** Vorzeichenbehaftete 32-Bit-Ganzzahl (Signed 32-Bit Integer), die die Millisekunden seit Beginn des Wettbewerbstages darstellt.


`xxRSqyyRSqzzRSq` soll so etwas sein: Gehörte Nachbarknoten im Mesh mit einem Byte das die Qualität der Linkverbinung beinhaltet. xx ist z.B. ein Knoten mit SSID xx, und q ist die Qualtität der Verbinung:
 q setzt sich zusammen aus: bit 7: 1 ... diese SSID (Nachbar) wurde in der letzten halben Stunde mindestens 3 mal empfangen.
 0 wenn das nicht der Fall ist (weniger als 3 mal gehört in den letzten 30 Minuten), aber zumindest einmal gehört, ansonsten verschwindet er aus der Liste.
 bit 6-0 Zahler wie oft die Station gehört wurde (gleitender Wert pro 30 Minuten).
 R ist der negative RSSI-Wert in unsigned char. und S entspricht dem vierfachen SNR-Wert (signed char).

 Beispiel: `0x00 03 9a 42 87...`

Das bedeutet "0x00 03" ist die SSID mit der Zahl 3 (der Nachbar der gehört wird), 0x9a ist dezimal  154
