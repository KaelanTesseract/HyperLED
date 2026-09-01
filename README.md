# HyperLED Project 🚀
<p align="center">
  <img width="1024" height="1024" alt="hyperled_logo" src="https://github.com/user-attachments/assets/9cf6acca-d60d-4139-bd9b-425fdc38ad3c" />
</p>

## Überblick

HyperLED ist eine hochperformante Open-Source-Firmware für adressierbare LED-Streifen und LED-Matrizen auf Basis des **ESP32-S3**. Sie richtet sich sowohl an einfache Anwendungen wie eine LED-Leiste hinter dem Fernseher als auch an anspruchsvolle Installationen mit mehreren synchronisierten Controllern oder großen HUB75-Matrixpanels.

Im Zentrum steht eine moderne, für Mobilgeräte optimierte Web-Oberfläche im Glassmorphism-Design, die direkt vom Controller ausgeliefert wird – keine Cloud, kein externer Dienst nötig. Über die kabelgebundene **HyperBus**-Verbindung lassen sich mehrere ESP32-Boards als Master/Slave-Verbund exakt synchron betreiben, sodass auch großflächige Installationen aus mehreren Segmenten wie ein einziges, durchgängiges System wirken.

## Hauptfunktionen

**Effekte & Steuerung**
- Über 25 eingebaute Animationseffekte (u. a. Feuer, Regenbogen, Plasma, Lauflicht, Konfetti) mit Geschwindigkeits-, Intensitäts- und Palettensteuerung
- Segmentierung: ein Streifen lässt sich in mehrere unabhängig steuerbare Zonen aufteilen
- Presets mit Playlist-Funktion sowie zeitgesteuerte Automatisierung (Schedules, zeitzonenbewusst)
- Anschluss physischer Taster/Schalter direkt am ESP32

**Matrix & Widgets**
- 2D-Matrix-Unterstützung (Serpentine- und fortlaufendes Layout) inklusive HUB75-Scan-Matrix-Panels
- Pixel-Art-Editor und -Konverter direkt in der Web-Oberfläche
- Widget-System für Uhrzeit, Datum, Text, Bilder und eine analoge Uhr mit mehreren Designs
- Wetter-Widget mit aktueller Temperatur und Wettersymbol (Open-Meteo, ohne API-Key)

**Master/Slave-Architektur (HyperBus)**
- Synchronisation mehrerer ESP32-Boards über eine kabelgebundene serielle Verbindung
- Automatische Erkennung neuer Slave-Boards inklusive Fernkonfiguration über die Master-Oberfläche
- Optionale kabellose Anbindung per ESP-NOW

**Netzwerk & Smart Home**
- WLAN-Einrichtung per Captive Portal, keine fest kodierten Zugangsdaten
- Erreichbar per mDNS (`hyperled.local`) und NetBIOS (`hyperled`) – funktioniert ohne Zusatzsoftware auch unter Windows
- MQTT-Integration mit Home-Assistant-Autodiscovery
- WLED-kompatible JSON-API für bestehende Automatisierungen und Drittanbieter-Tools
- Over-the-Air-Updates für Firmware und Web-Oberfläche über zwei Partitionen, direkt aus dem Browser

**Hardware & Sicherheit**
- Unterstützt eine breite Auswahl digitaler und analoger LED-Typen (WS281x-Familie, SK6812, APA102, PWM u. v. m.)
- Automatische Strombegrenzung (ABL) schützt Netzteil und Verkabelung vor Überlastung

## Hardware

HyperLED läuft auf dem **ESP32-S3** (getestet auf dem Waveshare ESP32-S3-Zero). Details zur Pinbelegung und Verkabelung stehen im Wiki.

## Dokumentation

- 🇩🇪 [**Deutsches Wiki lesen**](docs/de/01_Home.md)
- 🇬🇧 [**Read English Wiki**](docs/en/01_Home.md)

## Lizenz

HyperLED steht unter der [European Union Public Licence v1.2 (EUPL-1.2)](LICENSE). Hinweise zu verwendeten Drittanbieter-Bibliotheken finden sich am Ende der Lizenzdatei.

---
*Erstellt vom HyperLED-Team.*
