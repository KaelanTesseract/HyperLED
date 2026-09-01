# Hardware Setup & Pinbelegung

HyperLED läuft auf dem leistungsstarken **ESP32-S3**. Um optimale Ergebnisse zu erzielen, ist die richtige Verkabelung essenziell.

> [!IMPORTANT]
> Achte immer darauf, dass die Stromversorgung (Netzteil) stark genug für deine LEDs ist. Der ESP32 kann die LEDs NICHT direkt mit Strom versorgen (Gefahr der Beschädigung!). Verbinde immer nur die Datenleitung (DIN) und einen gemeinsamen Ground (GND) mit dem ESP32.

## Standard-Pinbelegung (Pinout)

In HyperLED sind bestimmte Pins ab Werk als Standard vorkonfiguriert. Du kannst diese später in der WebUI ändern.

| Funktion | ESP32-S3 Pin (Standard) | Beschreibung |
|---|---|---|
| **LED Datenleitung (Data)** | **GPIO 4** | Verbinde dies mit `DIN` oder `DI` deines LED-Streifens. |
| **Button 1 (Push)** | *Nicht aktiv* | Kann in der UI aktiviert werden, um physische Taster anzuschließen. Schaltet gegen GND. |
| **Button 2 (Switch)** | *Nicht aktiv* | Für Kippschalter gedacht. |

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
