# Installation & Flashen

Dieses Projekt nutzt **PlatformIO** (oft als Extension in Visual Studio Code) anstelle der Arduino IDE, um Abhängigkeiten und Speicherkonfigurationen (Partitions) perfekt zu managen.

## Voraussetzungen
1. Visual Studio Code installieren.
2. Die **PlatformIO IDE** Erweiterung in VS Code installieren.
3. Den HyperLED Quellcode herunterladen oder klonen.

## Schritt 1: Firmware Flashen (C++ Code)
Der C++ Code kümmert sich um die WLAN-Verbindung, die LED-Steuerung und stellt den Webserver bereit.

1. Öffne den Projektordner `HyperLED` in VS Code.
2. Schließe deinen ESP32-C6 per USB an den PC an.
3. Klicke in der unteren blauen Leiste (oder im PlatformIO Menü links) auf das kleine **Häkchen-Symbol (Build)**, um zu prüfen, ob alles korrekt kompiliert.
4. Klicke auf den **Pfeil nach rechts (Upload)**. Die Firmware wird nun auf den ESP32 geschrieben.

## Schritt 2: Web-Interface Flashen (LittleFS)
> [!WARNING]
> Wenn du diesen Schritt vergisst, wird der ESP32 zwar starten und im WLAN sein, aber im Browser nur "Not Found" oder einen weißen Bildschirm zeigen, da ihm die HTML/CSS-Dateien fehlen!

Das Web-Frontend liegt im Ordner `/data/` und muss in einen speziellen Speicherbereich (LittleFS) auf den ESP32 geladen werden.

1. Gehe in der linken Seitenleiste auf das **PlatformIO Icon** (Alien-Kopf).
2. Klappe unter "Project Tasks" deine Umgebung (z.B. `esp32-c6-devkitc-1`) auf.
3. Gehe zum Unterpunkt **Platform**.
4. Klicke zuerst auf **Build Filesystem Image**.
5. Klicke danach auf **Upload Filesystem Image**.

## Schritt 3: Erster Start (AP-Modus)
Nachdem beide Schritte erfolgreich waren:
1. Such auf deinem Smartphone nach neuen WLAN-Netzwerken.
2. Verbinde dich mit dem Netzwerk **HyperLED-AP**.
3. Ein Fenster ("Captive Portal") sollte sich automatisch öffnen. Falls nicht, öffne deinen Browser und gehe auf `http://192.168.4.1`.
4. Trage die WLAN-Zugangsdaten für dein Heimnetzwerk ein. Der ESP startet danach neu und verbindet sich mit deinem Router.
