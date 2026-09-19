<p align="center">
  <img width="320" alt="HyperLED" src="https://github.com/user-attachments/assets/9cf6acca-d60d-4139-bd9b-425fdc38ad3c" />
</p>

<h1 align="center">HyperLED</h1>

<p align="center">
  Open-Source-Firmware für adressierbare LED-Streifen und LED-Panels auf dem ESP32-S3 –
  <br>mit eigener Weboberfläche, Master/Slave-Verbund und Home-Assistant-Anbindung.
</p>

<p align="center">
  <a href="https://github.com/KaelanTesseract/HyperLED/releases/latest"><img alt="Neueste Version" src="https://img.shields.io/github/v/release/KaelanTesseract/HyperLED?label=Version"></a>
  <a href="LICENSE"><img alt="Lizenz" src="https://img.shields.io/badge/Lizenz-EUPL--1.2-blue"></a>
  <img alt="Plattform" src="https://img.shields.io/badge/Plattform-ESP32--S3-informational">
  <a href="docs/de/01_Home.md"><img alt="Dokumentation" src="https://img.shields.io/badge/Doku-Wiki-success"></a>
</p>

---

## Überblick

HyperLED steuert adressierbare LED-Streifen und LED-Matrizen – von der Lichtleiste hinter dem Fernseher bis zur großen Installation aus mehreren Controllern und HUB75-Panels.

Die Bedienung läuft über eine Weboberfläche, die der Controller selbst ausliefert: **keine Cloud, kein externer Dienst, kein Konto.** Mehrere Boards lassen sich über den **HyperBus** (Kabel oder Funk) zu einem Verbund zusammenschalten, der wie ein einziges durchgehendes System wirkt.

## Funktionen

**Licht und Effekte**
- Rund 30 Effekte (Feuer, Regenbogen, Plasma, Lauflicht, Konfetti …) mit Geschwindigkeit, Intensität und Farbpaletten
- Segmente: ein Streifen wird in mehrere unabhängig steuerbare Zonen aufgeteilt
- Szenen mit Playlist und zeitgesteuerte Abläufe (zeitzonenbewusst)
- Physische Taster und Schalter direkt am Controller

**Panels und Elemente**
- 2D-Matrizen (Serpentine und zeilenweise) sowie HUB75-Panels
- Elemente für Uhrzeit, Datum, Text, Lauftext, Bild, Wetter und eine analoge Uhr – frei platzierbar, mit eigener Helligkeit
- Hintergrund-Effekt hinter den Elementen, die dabei lesbar bleiben
- Pixel-Art-Editor und -Konverter in der Weboberfläche
- Wetter ohne API-Schlüssel (Open-Meteo)

**Master und Slaves (HyperBus)**
- Mehrere Controller laufen synchron, verbunden per Kabel (UART) oder Funk (ESP-NOW)
- Slaves werden automatisch erkannt und vom Master aus eingerichtet
- Slaves rechnen ihre Effekte selbst, das hält den Master frei
- Firmware-Update der Slaves aus der Ferne – die WLAN-Zugangsdaten bekommen sie dabei **verschlüsselt**

**Netzwerk und Smart Home**
- WLAN-Einrichtung über ein Captive Portal, keine fest eingetragenen Zugangsdaten
- Erreichbar über `hyperled.local` (mDNS) und NetBIOS, ohne Zusatzsoftware
- **Home Assistant** über MQTT mit Autodiscovery: ein Licht je Segment, Szenen, Texte fürs Panel, Effekt-Einstellungen, Diagnose-Sensoren, Taster als Auslöser, Firmware-Update und Neustart. Optional verschlüsselt (MQTTS).
- Online-Update für Firmware und Weboberfläche aus dem Browser oder aus Home Assistant – Szenen, Zeitpläne und Bilder bleiben dabei erhalten
- Sicherung aller Einstellungen als Datei, wiederherstellbar auch auf einem anderen Controller

**Hardware**
- Viele LED-Typen: WS281x-Familie, SK6812, APA102, WS2805, SM16825, PWM-Kanäle und mehr
- Automatische Strombegrenzung (ABL) schützt Netzteil und Verkabelung

## Hardware

Entwickelt und getestet auf dem **ESP32-S3** (Waveshare ESP32-S3-Zero, 4 MB Flash, 2 MB PSRAM). Welche anderen ESP32-Chips in Frage kommen und was dafür anzupassen wäre, steht unter [Hardware Setup](docs/de/03_Hardware_Setup.md).

Nötig sind mindestens 4 MB Flash, WLAN mit ESP-NOW und – für HUB75-Panels – ein Chip, den die Panel-Bibliothek unterstützt.

> [!IMPORTANT]
> Der ESP32 darf die LEDs nicht mit Strom versorgen. Immer ein passendes Netzteil verwenden und nur Datenleitung und gemeinsame Masse mit dem Controller verbinden.

## Installation

Gebaut und geflasht wird mit **PlatformIO** (Erweiterung für Visual Studio Code).

```bash
git clone https://github.com/KaelanTesseract/HyperLED.git
cd HyperLED

pio run -t upload      # Firmware auf den ESP32 schreiben
pio run -t uploadfs    # Weboberfläche (Ordner /data) schreiben
```

Beide Schritte sind nötig. Ohne den zweiten startet der Controller zwar, zeigt im Browser aber nur „Not Found“.

**Erste Einrichtung**
1. Nach dem Start öffnet der Controller das WLAN **HyperLED-AP**.
2. Damit verbinden – die Einrichtungsseite öffnet sich von selbst, sonst `http://192.168.4.1` aufrufen.
3. Das eigene WLAN auswählen und das Passwort eintragen. Der Controller startet neu und ist danach unter `http://hyperled.local` erreichbar.

Ausführlich mit Bildern: [Installation & Flashen](docs/de/04_Installation_Flash.md).

> [!NOTE]
> Die Dateien in den [Releases](https://github.com/KaelanTesseract/HyperLED/releases) sind für das Online-Update gedacht. Die Erstinstallation läuft über den Quellcode.

**Aktualisieren**

Spätere Versionen lassen sich direkt in der Weboberfläche unter *Einstellungen → System* installieren, oder in Home Assistant über die Firmware-Entität. Szenen, Playlist, Zeitpläne und Bilder bleiben erhalten.

## Dokumentation

- 🇩🇪 [Deutsches Wiki](docs/de/01_Home.md)
- 🇬🇧 [English Wiki](docs/en/01_Home.md)

Darin: [Funktionen](docs/de/02_Features.md) · [Hardware und Pinbelegung](docs/de/03_Hardware_Setup.md) · [Installation](docs/de/04_Installation_Flash.md) · [Weboberfläche und MQTT](docs/de/05_Web_Interface.md) · [Master/Slave](docs/de/07_Master_Slave_Architektur.md) · [API-Referenz](docs/de/08_API_Referenz.md)

## Mitmachen

Fehlerberichte und Vorschläge gern über die [Issues](https://github.com/KaelanTesseract/HyperLED/issues). Die Firmware der Slave-Boards liegt in einem eigenen Repository: [HyperLED-Slave](https://github.com/KaelanTesseract/HyperLED-Slave).

## Lizenz

[European Union Public Licence v1.2 (EUPL-1.2)](LICENSE). Die verwendeten Drittanbieter-Bibliotheken sind am Ende der Lizenzdatei aufgeführt.
