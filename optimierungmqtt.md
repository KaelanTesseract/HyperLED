# HyperLED MQTT / Home Assistant – Optimierungsplan

Grundlage: Durchsicht der MQTT-Anbindung (`src/MqttManager.cpp`) im September 2026. HyperLED soll
über MQTT mit Home Assistant verbunden werden (Autodiscovery, JSON-Schema).

**Regel:** Ist ein Punkt erledigt, wird er aus dieser Datei gelöscht – hier steht nur, was noch
offen ist. Neu entdeckte Folgearbeiten kommen ebenfalls hierher.

## B. Neue Möglichkeiten in Home Assistant

1. **Texte auf dem Panel aus Home Assistant setzen** – Text-Entität für Lauftext und
   Text-Elemente, z. B. für Benachrichtigungen wie „Waschmaschine fertig“ auf dem Panel.
2. **Szenen und Playlist** – Szenen (Presets) als Auswahl (`select`), Playlist als Schalter.
3. **Effekt-Feinheiten pro Segment** – Geschwindigkeit und Intensität als Regler (`number`),
   Palette als Auswahl (`select`).
4. **Gleichlauf** als Schalter.
5. **Panel-Hintergrund-Effekt** als Auswahl.
6. **Diagnose-Sensoren** – WLAN-Signal, Laufzeit, freier Speicher, IP, Firmware, Anzahl und
   Zeitpunkt des letzten Funk-Ausfalls. Home Assistant kann dann bei einem Ausfall benachrichtigen.
7. **Status der verbundenen Geräte** – verbunden/getrennt je Slave (`binary_sensor`), dazu deren
   Firmware-Version.
8. **Taster als Auslöser** – die physischen Taster am Controller lösen Home-Assistant-Automationen
   aus (Geräte-Trigger bzw. `event`).
9. **Firmware-Update in Home Assistant** – `update`-Entität: zeigt ein verfügbares Update und
   installiert es auf Knopfdruck.
10. **Neustart-Knopf** (`button`); optional die Onboard-LED als eigenes Licht.

## C. Weboberfläche

1. **MQTT-Verbindungsstatus anzeigen** – verbunden / nicht verbunden mit Fehlergrund, dazu ein
   Knopf „Verbindung testen“.
2. **Knopf „In Home Assistant neu anmelden“** – sendet Discovery und Zustand erneut.
3. **Kurzanleitung in der Doku** – Einrichtung in Home Assistant Schritt für Schritt (de/en).

## D. Stabilität und Aufräumen (bei der Umsetzung von A gefunden)

1. **Verbindungsaufbau aus der Hauptschleife nehmen** – `connect()` blockiert die Hauptschleife,
   solange der Broker nicht antwortet (auf 1 s begrenzt, mit Hostname plus DNS-Auflösung länger);
   die LEDs stehen in der Zeit. Bei ausgeschaltetem Broker passiert das höchstens einmal pro Minute.
   Sauber wäre ein eigener Task für Verbindungsaufbau und Namensauflösung.
2. **MQTT abschalten räumt Home Assistant nicht auf** – nach dem Deaktivieren bleiben die Lichter in
   Home Assistant als „nicht verfügbar“ stehen. Vor dem Neustart die Discovery-Einträge leeren
   (leere retained-Nachrichten), damit das Gerät verschwindet.

## Optional

- **Verschlüsselte Verbindung (MQTTS, Port 8883)** – kostet Speicher und ist im Heimnetz meist
  unnötig; nur umsetzen, wenn der Broker es verlangt.
