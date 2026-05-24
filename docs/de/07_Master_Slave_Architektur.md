# Master/Slave Architektur

Eines der mächtigsten Features von HyperLED ist die Fähigkeit, mehrere ESP32-Controller extrem zuverlässig und synchron miteinander zu verbinden. Dies geschieht **kabelgebunden** über ein hochperformantes, serielles Protokoll namens **HyperBus** (über die UART Pins).

## Wie es funktioniert
Ein Controller agiert als **Master** (Sender). Andere Controller in der Kette agieren als **Slave**.
Der Master sendet bei jeder Frame-Berechnung die rohen Farbdaten aller LEDs als hochgeschwindigkeits-serielles Paket (HyperBus) an die Slaves. Die Slaves lauschen auf diesen Stream und leiten ihn 1:1 auf ihre eigenen LED-Streifen weiter.

### Verkabelung (UART)
Für die Kommunikation müssen Master und Slaves mit einer Datenleitung (+ gemeinsamem Ground) verbunden werden:
- **Master TX** (Standard: GPIO 21) verbindet sich mit **Slave RX** (Standard: GPIO 20).
- Alle verbundenen Platinen müssen zwingend den gleichen **Ground (GND)** teilen!

## Einrichtung
1. Verbinde die Hardware-Pins wie oben beschrieben.
2. Öffne das Web-Interface deines Haupt-Controllers.
3. Die Slaves am HyperBus melden sich beim Master automatisch (Ping/Pong Protokoll).
4. Gehe in den Tab **LEDs**.
5. Erstelle sogenannte **Slave Segmente**. Hierbei handelt es sich um virtuelle Abschnitte, deren berechnete Daten exklusiv über den HyperBus an die entsprechenden Slave-IDs gesendet werden.

> [!TIP]
> **Freeze-Funktion:** Ein Slave kann über einen API-Befehl "eingefroren" werden (`/api/slaves?cmd=freeze`). In diesem Zustand ignoriert er den Master-Stream vorübergehend. So kannst du z.B. einen Raum synchron schalten, aber einzelne Vitrinen kurzzeitig für eine andere Aktion entkoppeln.
