# Die Android Companion App

HyperLED verfügt über eine maßgeschneiderte Android App (geschrieben in Kotlin). Sie dient als Wrapper für das Web-Interface, fügt aber tiefgreifende native Features hinzu.

## Features der App
1. **Auto-Discovery (mDNS):** Du musst die IP-Adresse des ESP32 nicht mehr kennen! Die App scannt dein WLAN automatisch nach Geräten, die den Service `_wled._tcp.` bereitstellen.
2. **AP-Modus Schnellstart:** Ein blauer Button ganz oben erlaubt es dir, ein komplett neues Gerät (welches noch nicht in deinem WLAN ist) mit einem Klick zu konfigurieren, wenn du mit seinem Hotspot verbunden bist.
3. **Geräte-Liste mit Status:**
   - **Name:** Standardmäßig wird der interne Hostname angezeigt. Klickst du darauf, öffnet sich das Vollbild-Webinterface.
   - **Power-Toggle:** Ein kleines Icon zeigt den Live-Status an (Grün = An, Grau = Aus). Mit einem Klick schaltest du die LEDs ein oder aus – sofort, ohne die UI laden zu müssen.
   - **Umbenennen (Stift-Symbol):** Klicke auf den Stift, um dem Gerät einen eigenen Namen zu geben (z.B. "Gaming Zimmer"). Dieser wird lokal auf dem Smartphone dauerhaft gespeichert.

## App Kompilieren & Installieren
Die App liegt im Ordner `HyperLED_Android`.
1. Öffne **Android Studio**.
2. Wähle *File -> Open* und navigiere zum Ordner `HyperLED_Android`.
3. Warte, bis Gradle den Sync abgeschlossen hat (das Elefanten-Icon oben rechts).
4. Schließe dein Android-Smartphone an (USB-Debugging muss am Handy aktiviert sein) oder starte einen Emulator.
5. Drücke auf den grünen **Run** (Play) Button.
