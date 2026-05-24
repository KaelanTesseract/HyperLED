# Hardware Setup & Pinbelegung

HyperLED läuft auf dem leistungsstarken **ESP32-C6**. Um optimale Ergebnisse zu erzielen, ist die richtige Verkabelung essenziell.

> [!IMPORTANT]
> Achte immer darauf, dass die Stromversorgung (Netzteil) stark genug für deine LEDs ist. Der ESP32 kann die LEDs NICHT direkt mit Strom versorgen (Gefahr der Beschädigung!). Verbinde immer nur die Datenleitung (DIN) und einen gemeinsamen Ground (GND) mit dem ESP32.

## Standard-Pinbelegung (Pinout)

In HyperLED sind bestimmte Pins ab Werk als Standard vorkonfiguriert. Du kannst diese später in der WebUI ändern.

| Funktion | ESP32-C6 Pin (Standard) | Beschreibung |
|---|---|---|
| **LED Datenleitung (Data)** | **GPIO 4** | Verbinde dies mit `DIN` oder `DI` deines LED-Streifens. |
| **Button 1 (Push)** | *Nicht aktiv* | Kann in der UI aktiviert werden, um physische Taster anzuschließen. Schaltet gegen GND. |
| **Button 2 (Switch)** | *Nicht aktiv* | Für Kippschalter gedacht. |

## Unterstützte LED-Typen

Die Firmware nutzt modernste Bibliotheken, um eine Vielzahl von digitalen LED-Chips anzusteuern. 

> [!WARNING]
> **Teststatus beachten:** Da Hardware-Tests aufwendig sind, sind bisher nicht alle Chips physisch verifiziert worden.

### Getestet & 100% Empfohlen:
- **WS281X-Familie:**
  - **WS2812B** (Der absolute Standard, 5V)
  - **WS2813** (Mit Backup-Datenleitung)
  - **WS2815** (12V Version, ideal für lange Strecken ohne Spannungseinbruch)

### Unterstützt (Aber ungetestet):
- **SK6812** (Oft mit dediziertem Weiß-Kanal, z.B. RGBW)
- **APA102 / SK9822** (Benötigt Daten- UND Clock-Leitung - Pin in UI konfigurierbar)
- **WS2801** (Älterer 4-Pin Standard)
- **LPD8806** 

## Best Practices für die Verkabelung
1. **Level-Shifter:** Der ESP32 gibt auf den Datenpins 3.3V aus. Viele LEDs (wie WS2812B) erwarten offiziell 5V Logik. Oft funktioniert es ohne, aber bei Flackern solltest du einen Logikpegelwandler (Level Shifter, z.B. 74AHCT125) verwenden.
2. **Kondensator:** Schließe einen großen Kondensator (z.B. 1000 µF, 6.3V oder höher) parallel zur Stromversorgung (VCC und GND) des LED-Streifens an, um Stromspitzen beim Einschalten abzufangen.
3. **Widerstand:** Setze einen Widerstand (ca. 330 bis 470 Ohm) in die Datenleitung zwischen dem ESP32-Pin (GPIO 4) und dem ersten LED-Chip. Das schützt den Pin vor Spannungsspitzen.
