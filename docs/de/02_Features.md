# Funktionen & Features

HyperLED bietet eine riesige Bandbreite an Funktionen, die das System sowohl für simple LED-Streifen hinter dem Fernseher als auch für riesige Matrix-Installationen oder ganze Raum-Beleuchtungen prädestinieren.

## Kernfunktionen
* **Extrem flüssige Effekte:** Bis zu 100 FPS Berechnung, selbst bei vielen LEDs.
* **Master/Slave Architektur (HyperBus):** Steuere mehrere ESP32 Boards perfekt synchron über eine zuverlässige, kabelgebundene serielle Verbindung (TX/RX).
* **Segmente:** Teile einen einzelnen LED-Streifen in mehrere virtuelle Zonen auf. Jede Zone kann einen eigenen Effekt, eine eigene Farbe und Geschwindigkeit haben.
* **Pixel Art Converter:** Lade Pixel-Art Bilder (PNG, JPG) hoch, und der ESP32 rechnet diese sofort für LED-Matrizen um.

## Steuerung
* **Modernes Web-Interface:** Die UI nutzt "Glassmorphism" und ist stark für mobile Geräte (Smartphones) optimiert. Sie läuft als moderne Single-Page-Application rasend schnell direkt aus dem LittleFS des ESP32.
* **Android Companion App:** Finde alle deine HyperLED-Controller automatisch im Netzwerk. Ändere Namen, schalte sie per Direkt-Knopf an und aus, ohne IP-Adressen eintippen zu müssen.
* **Tastensteuerung (Physisch):** Unterstützt den Anschluss von Push-Buttons oder Schaltern direkt an die ESP32 GPIO-Pins.

## Netzwerk & Smart Home
* **mDNS Support:** Erreichbar unter `http://hyperled.local` (oder deinem vergebenen Namen).
* **WLAN Manager & Captive Portal:** Keine fest kodierten Passwörter mehr. Richte das WLAN einfach über dein Smartphone ein, wenn der ESP32 im Access-Point (AP) Modus startet.
* **MQTT:** Volle Integration für Home Assistant, ioBroker und andere Smart Home Zentralen. Sende JSON-Pakete an den Controller, um ihn komplett fernzusteuern.
* **Over-The-Air (OTA) Updates:** Update die Firmware oder die WebUI über eine elegante Lade-Animation direkt im Browser, ohne den ESP32 an den PC anschließen zu müssen.
