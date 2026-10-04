# Plugins

Beispiele und das JSON-Schema für HyperLED-Plugins. Ein Plugin ist eine einzelne JSON-Datei, die einen Wert aus dem Netzwerk liest und auf einem Segment anzeigt. Installiert wird sie in der Weboberfläche unter **Einstellungen → Plugins** (als Datei oder von einer Adresse).

| Datei | Zeigt |
|---|---|
| [`beispiel-minimal.json`](beispiel-minimal.json) | Das kleinste sinnvolle Plugin: rot, wenn es in Berlin über 25 Grad warm ist, sonst blau. Zum Lernen. |
| [`beispiel-wetter.json`](beispiel-wetter.json) | Einstellungen für Ort, Schwellen, Farben und einen wählbaren Effekt; Fehlerzustand. |
| [`beispiel-skript-thermometer.json`](beispiel-skript-thermometer.json) | Ein Lua-Skript zeichnet ein Thermometer über das ganze Segment. |
| [`plugin.schema.json`](plugin.schema.json) | JSON-Schema (Draft 2020-12) für Editoren: Vervollständigung und Warnungen beim Schreiben eigener Plugins. |

Die Beispiele fragen die kostenlose Schnittstelle [Open-Meteo](https://open-meteo.com/) ab (ohne Anmeldung, für private Nutzung) und laufen deshalb ohne eigenen Server.

Dokumentation: [Plugins nutzen](../docs/de/09_Plugins_nutzen.md) · [Plugins entwickeln](../docs/de/10_Plugins_entwickeln.md) · [Plugin-Skripte](../docs/de/11_Plugin_Skripte.md) (auch auf [Englisch](../docs/en/01_Home.md)).

Lizenz: [EUPL-1.2](../LICENSE), wie der Rest des Projekts.
