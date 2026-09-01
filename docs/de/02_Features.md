# Funktionen & Features

HyperLED bietet eine riesige Bandbreite an Funktionen, die das System sowohl für simple LED-Streifen hinter dem Fernseher als auch für riesige Matrix-Installationen oder ganze Raum-Beleuchtungen prädestinieren.

## Kernfunktionen
* **Extrem flüssige Effekte:** Über 25 eingebaute Animationseffekte (Feuer, Regenbogen, Plasma, Lauflicht, Konfetti u. v. m.) mit bis zu 100 FPS Berechnung, selbst bei vielen LEDs.
* **Master/Slave Architektur (HyperBus):** Steuere mehrere ESP32-Boards perfekt synchron – kabelgebunden über eine zuverlässige serielle Verbindung (TX/RX) oder kabellos über ESP-NOW.
* **Segmente:** Teile einen einzelnen LED-Streifen in mehrere virtuelle Zonen auf. Jede Zone kann einen eigenen Effekt, eine eigene Farbe und Geschwindigkeit haben.
* **Presets & Playlists:** Speichere komplette Lichtstimmungen als Preset und lass mehrere Presets automatisch nacheinander abspielen.
* **Zeitpläne:** Schalte Effekte oder Presets zeitgesteuert (zeitzonenbewusst über NTP).

## Matrix & Widgets
* **2D-Matrix-Unterstützung:** Serpentine- und fortlaufendes Layout, inklusive HUB75-Scan-Matrix-Panels.
* **Pixel Art Editor & Converter:** Male direkt im Browser oder lade Pixel-Art-Bilder hoch – beides landet als Hintergrundbild-Widget auf der Matrix.
* **Widget-System:** Uhrzeit, Datum, Text, Bilder und eine analoge Uhr (mehrere Designs) lassen sich frei auf der Matrix platzieren und einzeln skalieren, mit Live-Vorschau ohne Zwischenschritt.
* **Wetter-Widget:** Zeigt aktuelle Temperatur und Wettersymbol (Sonne, Wolken, Regen, Schnee, Gewitter) an – auf Basis von Open-Meteo, ohne API-Key.

## Steuerung
* **Modernes Web-Interface:** Die UI nutzt "Glassmorphism" und ist stark für mobile Geräte (Smartphones) optimiert. Sie läuft als moderne Single-Page-Application rasend schnell direkt aus dem LittleFS des ESP32.
* **Tastensteuerung (Physisch):** Unterstützt den Anschluss von Push-Buttons oder Schaltern direkt an die ESP32-GPIO-Pins.

## Netzwerk & Smart Home
* **mDNS & NetBIOS Support:** Erreichbar unter `http://hyperled.local` (macOS/iOS/Linux/Android) oder `http://hyperled` (Windows, per NetBIOS – funktioniert ohne zusätzliche Software wie Bonjour).
* **WLAN Manager & Captive Portal:** Keine fest kodierten Passwörter mehr. Richte das WLAN einfach über dein Smartphone ein, wenn der ESP32 im Access-Point (AP) Modus startet.
* **MQTT:** Volle Integration für Home Assistant, ioBroker und andere Smart-Home-Zentralen, inklusive Home-Assistant-Autodiscovery.
* **WLED-kompatible JSON-API:** Bestehende WLED-Automatisierungen und Drittanbieter-Tools funktionieren größtenteils ohne Anpassung.
* **Over-The-Air (OTA) Updates:** Update Firmware und Web-Oberfläche direkt im Browser, ohne den ESP32 an den PC anschließen zu müssen.

## Stromversorgung
* **Automatische Strombegrenzung (ABL):** Schätzt den Stromverbrauch aller LEDs live und begrenzt die Helligkeit automatisch, um das Netzteil vor Überlastung zu schützen.
