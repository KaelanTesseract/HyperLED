# Hardware Setup & Pinbelegung

HyperLED läuft auf dem leistungsstarken **ESP32-S3**. Um optimale Ergebnisse zu erzielen, ist die richtige Verkabelung essenziell.

> [!IMPORTANT]
> Achte immer darauf, dass die Stromversorgung (Netzteil) stark genug für deine LEDs ist. Der ESP32 kann die LEDs NICHT direkt mit Strom versorgen (Gefahr der Beschädigung!). Verbinde immer nur die Datenleitung (DIN) und einen gemeinsamen Ground (GND) mit dem ESP32.

## Welche ESP32-Chips eignen sich?

HyperLED ist für den **ESP32-S3** entwickelt und nur darauf getestet (Master und Slave: Waveshare ESP32-S3-Zero). In der Theorie laufen auch andere ESP32-Chips – vorausgesetzt, sie bringen mit, was HyperLED braucht:

- **WLAN (2,4 GHz) und ESP-NOW** – für Weboberfläche, MQTT, Online-Update und Funk-Slaves
- **mindestens 4 MB Flash** – zwei Update-Plätze à 1,44 MB plus 1,06 MB Dateisystem; die Master-Firmware füllt ihren Platz schon zu gut 90 %
- **genug Arbeitsspeicher** für Webserver, TLS und ESP-NOW gleichzeitig (als Master)
- **für HUB75-Panels:** einen Chip, den die Panel-Bibliothek `esp-hub75` unterstützt (ESP32, S2, S3, C6, P4)

### Geeignet

| Chip | Kerne / RAM | Master | Slave | HUB75 | Anmerkung |
|---|---|---|---|---|---|
| **ESP32-S3** (N4R2, N8R2, N8R8, N16R8 …) | 2 × 240 MHz, 512 KB + PSRAM | ✅ | ✅ | ✅ | **Die Referenz-Plattform.** Beste Wahl, besonders mit PSRAM. |
| **ESP32** (klassisch: WROOM-32, WROVER) | 2 × 240 MHz, 520 KB (WROVER + PSRAM) | ✅ | ✅ | ✅ | Leistung reicht. Andere Pinbelegung (GPIO 34–39 nur Eingang, 6–11 belegt), kein natives USB. |
| **ESP32-S2** (z. B. S2 Mini mit PSRAM) | 1 × 240 MHz, 320 KB (+ PSRAM) | ⚠️ knapp | ✅ | ✅ | Nur ein Kern, wenig RAM – als Master nur mit PSRAM sinnvoll. |
| **ESP32-C6** | 1 × 160 MHz, 512 KB | ⚠️ | ✅ | ✅ | Langsamer, ohne PSRAM. |
| **ESP32-C3** | 1 × 160 MHz, 400 KB | ⚠️ | ✅ | ❌ | Nur LED-Streifen, kein Panel. Als günstiger Slave gut geeignet. |
| **ESP32-C5** | 1 × 240 MHz, PSRAM möglich | ⚠️ theoretisch | ⚠️ theoretisch | ❌ | Neu, WLAN auch auf 5 GHz; im Arduino-Core erst seit Kurzem, ungetestet. |

✅ geeignet · ⚠️ möglich, aber mit Einschränkungen · ❌ nicht möglich

### Nicht geeignet

| Chip | Grund |
|---|---|
| **ESP32-P4** | Kein eigenes WLAN; nur mit Funk-Zusatzchip (z. B. C6 über ESP-Hosted) denkbar, ESP-NOW darüber fraglich. |
| **ESP32-H2 / H4** | Kein WLAN (nur Bluetooth und Thread/Zigbee). |
| **ESP32-C2 (ESP8684)** | Wenig RAM, meist nur 2–4 MB Flash, im Arduino-Core nicht offiziell unterstützt. |
| **Module mit 2 MB Flash** | Zu wenig Platz für zwei Update-Plätze. |
| **ESP8266** | Kein ESP32, eigene Plattform. |

### Was für einen anderen Chip angepasst werden muss

1. **Pins:** Die festen Pins in `include/Config.h` (HUB75, HyperBus-UART, Taster, Status-LED) sind auf den ESP32-S3 zugeschnitten.
2. **Build-Umgebung:** ein eigenes `[env]` in `platformio.ini`; die USB-CDC-Einstellungen gelten nur für Chips mit nativem USB.
3. **Chips mit einem Kern (S2, C3, C5, C6):** Die Hintergrund-Tasks für MQTT-Verbindung, Verbindungstest und Update-Prüfung sind fest auf Kern 1 gelegt und müssten dort auf den einzigen Kern verlegt werden.
4. **Arbeitsspeicher:** Auf Chips mit einem Kern ohne PSRAM wird es als Master mit TLS (MQTTS, Online-Update) und Webserver eng; als Slave ist das kein Problem.

**Empfehlung:** Master auf einem **ESP32-S3 mit PSRAM**, zur Not ein klassischer ESP32 (WROVER). Slaves ohne Panel können günstig auf einem **ESP32-C3** laufen, Slaves mit Panel auf einem S3 oder C6.

## Standard-Pinbelegung (Pinout)

In HyperLED sind bestimmte Pins ab Werk als Standard vorkonfiguriert. Du kannst diese später in der WebUI ändern.

| Funktion | ESP32-S3 Pin (Standard) | Beschreibung |
|---|---|---|
| **LED Datenleitung (Data)** | **GPIO 4** | Verbinde dies mit `DIN` oder `DI` deines LED-Streifens. In der WebUI frei wählbar. |
| **Eingang 1 (Taster)** | **GPIO 39** | Fest verdrahtet, muss in der WebUI aktiviert werden. Schaltet gegen GND. |
| **Eingang 2 (Schalter)** | **GPIO 40** | Fest verdrahtet, muss in der WebUI aktiviert werden. Für Kippschalter gedacht. |

> [!NOTE]
> Für ein HUB75-Matrixpanel ist die Pinbelegung fest verdrahtet (14 gleichzeitig benötigte Signale) und wird beim Auswählen des LED-Typs **HUB75** direkt in der WebUI als Referenz angezeigt.

## Unterstützte LED-Typen

Die Firmware nutzt die Bibliothek [NeoPixelBus](https://github.com/Makuna/NeoPixelBus), um eine Vielzahl von digitalen LED-Chips anzusteuern.

> [!WARNING]
> **Teststatus beachten:** Nur die WS281X-Familie unten ist auf echter Hardware verifiziert. Alles andere wurde (Stand 2026-08-27) *im Code gegengelesen*: Für jeden Chip wurde geprüft, ob die passenden NeoPixelBus-Feature-/Method-Klassen zum im Datenblatt dokumentierten Protokoll passen — aber nicht physisch getestet. Ein Code-Review findet Protokoll-Fehlanpassungen (siehe unten), aber keine hardwarespezifischen Verkabelungs- oder Timing-Eigenheiten.

### Getestet & 100% Empfohlen:
- **WS281X-Familie:**
  - **WS2812B** (Der absolute Standard, 5V)
  - **WS2813** (Mit Backup-Datenleitung)
  - **WS2815** (12V Version, ideal für lange Strecken ohne Spannungseinbruch)

### Code-verifiziert (Treiber gegen NeoPixelBus/Datenblatt geprüft, noch nicht hardware-getestet):
- **SK6812** (Oft mit dediziertem Weiß-Kanal, z.B. RGBW)
- **APA102 / SK9822** (Benötigt Daten- UND Clock-Leitung - Pin in UI konfigurierbar)
- **WS2801** (Älterer 4-Pin Standard, einfaches RGB über SPI)
- **LPD8806**
- **LPD6803** (16-Bit-Wort-SPI-Protokoll)
- **P9813 / PP9813** (Eigenes Prüfsummen-Framing pro Pixel)
- **TM1814**, **TM1914** (Benötigen einen chip-spezifischen Einstellungs-Header vor den Pixeldaten)
- **TM1829, UCS8903, APA106, WS2811 (White), WS281X WWA** (Werden als WS2812-protokollkompatible Klone behandelt)
- **FW1906, UCS8904** (RGBW-Klone)
- **WS2805, SM16825** (RGBCW-Klone mit Warm- und Kaltweiß-Kanal)
- **On/Off, PWM (1-5-Kanal analog/CCT)**

> [!NOTE]
> **Bug behoben am 2026-08-27:** WS2801, LPD6803 und P9813 wurden in der Firmware bisher fälschlicherweise mit dem APA102/DotStar-Protokolltreiber angesteuert — ein echter Protokoll-Mismatch (komplett anderes SPI-Framing), der diese drei Chip-Typen auf echter Hardware vermutlich funktionsunfähig gemacht hätte. TM1914 fehlte der benötigte Einstellungs-Header. Beides ist jetzt behoben; wer einen dieser Streifen besitzt, bitte neu flashen und Rückmeldung geben, ob es funktioniert.

## Best Practices für die Verkabelung
1. **Level-Shifter:** Der ESP32 gibt auf den Datenpins 3.3V aus. Viele LEDs (wie WS2812B) erwarten offiziell 5V Logik. Oft funktioniert es ohne, aber bei Flackern solltest du einen Logikpegelwandler (Level Shifter, z.B. 74AHCT125) verwenden.
2. **Kondensator:** Schließe einen großen Kondensator (z.B. 1000 µF, 6.3V oder höher) parallel zur Stromversorgung (VCC und GND) des LED-Streifens an, um Stromspitzen beim Einschalten abzufangen.
3. **Widerstand:** Setze einen Widerstand (ca. 330 bis 470 Ohm) in die Datenleitung zwischen dem ESP32-Pin (GPIO 4) und dem ersten LED-Chip. Das schützt den Pin vor Spannungsspitzen.
