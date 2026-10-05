# Plugins entwickeln

Ein Plugin ist **eine JSON-Datei**. Sie beschreibt vier Dinge:

1. **Einstellungen** (`settings`): was die Person einträgt, zum Beispiel eine Adresse oder eine Farbe.
2. **Eine Quelle** (`source`): welche Adresse HyperLED abfragt.
3. **Werte** (`values`): welche Teile der Antwort wichtig sind.
4. **Regeln** (`rules`): was auf dem Segment zu sehen ist, abhängig von den Werten.

Wer mehr als Effekt, Farbe und Geschwindigkeit braucht, ergänzt ein **Skript** (Lua, siehe [Plugin-Skripte](11_Plugin_Skripte.md)). Alles in diesem Kapitel gilt auch dann.

Du brauchst **keine Software außer einem Texteditor** und einem HyperLED im Netzwerk: Die Installation prüft die Datei sofort und nennt Fehler mit der Stelle. Ein JSON-Schema für Editoren liegt unter [`plugins/plugin.schema.json`](../../plugins/plugin.schema.json).

---

## In fünf Schritten zum ersten Plugin

Die Beispiele aus dem Ordner [`plugins/`](../../plugins/) fragen die kostenlose Wetter-Schnittstelle [Open-Meteo](https://open-meteo.com/) ab (ohne Anmeldung, für private Nutzung); du kannst sie sofort ausprobieren.

### 1. Das Gerüst

```json
{
  "format": 1,
  "id": "mein-plugin",
  "name": "Mein erstes Plugin",
  "version": "1.0.0",
  "license": "EUPL-1.2",
  "source": { "url": "https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m" }
}
```

`format` ist immer `1`. `id` ist der eindeutige Name (nur `a-z`, `0-9`, `-`, `_`). `license` ist Pflicht: Sag, unter welchen Bedingungen dein Plugin weitergegeben werden darf. Die Quelle ist eine Adresse, die HyperLED mit **GET** abfragt und die JSON liefert.

### 2. Werte aus der Antwort lesen

Die Antwort der Quelle sieht so aus:

```json
{ "current": { "time": "2026-10-04T15:30", "temperature_2m": 19.4 } }
```

`values` gibt Namen für das, was du brauchst. Der Ausdruck rechts ist der **Pfad** in die Antwort:

```json
"values": { "temp": "current.temperature_2m" }
```

### 3. Regeln: was soll angezeigt werden?

```json
"rules": [
  { "when": "temp > 25",  "show": { "effect": "Einfarbig", "color": "#ff3300" } },
  { "when": "temp <= 25", "show": { "effect": "Einfarbig", "color": "#0066ff" } }
]
```

Die Regeln werden **von oben nach unten** geprüft; die **erste**, deren `when` stimmt, gilt. Passt keine, bleibt das Segment, wie die Person es eingestellt hat.

### 4. Das Segment: eine Einstellung

Das Plugin zeigt etwas auf einem Segment an, also braucht es eine Einstellung vom Typ `segment`, in der die Person das Segment auswählt:

```json
"settings": [
  { "key": "segment", "type": "segment", "label": { "de": "Segment", "en": "Segment" } }
]
```

Fertig: Das ist [`beispiel-minimal.json`](../../plugins/beispiel-minimal.json). Installiere es unter **Einstellungen → Plugins**, wähle ein Segment und schalte es ein.

### 5. Einstellungen für alles, was die Person ändern soll

Statt `52.52` und `13.41` fest in die Adresse zu schreiben, lässt du die Person ihren Ort eintragen:

```json
"settings": [
  { "key": "lat", "type": "number", "default": 52.52, "min": -90, "max": 90, "label": "Breitengrad" },
  { "key": "lon", "type": "number", "default": 13.41, "min": -180, "max": 180, "label": "Längengrad" },
  { "key": "segment", "type": "segment", "label": "Segment" }
],
"source": { "url": "https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m" }
```

`{lat}` in der Adresse wird mit dem Wert der Einstellung `lat` ersetzt. Schwellen und Farben werden genauso Einstellungen; in den Regeln benutzt du sie wie Werte (`temp <= cold`) oder mit geschweiften Klammern (`"color": "{c_warm}"`). Das vollständige Beispiel mit Schwellen, Farben, wählbarem Effekt und Fehlerzustand ist [`beispiel-wetter.json`](../../plugins/beispiel-wetter.json).

---

## Prüfen und ausprobieren

- **Vorschau ohne Speichern:** `POST /api/plugins/preview` prüft eine Datei vollständig und speichert nichts:

  ```bash
  curl -s -X POST -H "Content-Type: application/json" --data-binary @mein-plugin.json http://<IP-des-Controllers>/api/plugins/preview
  ```

  Bei Erfolg kommen Name, Adresse und Verträglichkeit zurück, bei einem Fehler `{"error": "…"}` mit der Stelle.
- **Live-Werte:** In der Oberfläche zeigt **Live-Werte**, was das Plugin liest, welche Regel gerade gilt und den Anfang der Antwort – ohne ein Segment zu ändern. Dasselbe liefert `GET /api/plugins/values?id=<id>`.
- **Die eigene Quelle ansehen:** Ruf die Adresse mit `curl` auf und sieh dir die Antwort an. Pfade, die es dort nicht gibt, ergeben „unbekannt“.
- **Editor:** Das [JSON-Schema](../../plugins/plugin.schema.json) gibt dir Vervollständigung und Warnungen. In Visual Studio Code zum Beispiel in den Einstellungen unter `json.schemas`:

  ```json
  "json.schemas": [ { "fileMatch": ["**/*.plugin.json"], "url": "./plugins/plugin.schema.json" } ]
  ```

  Schreibe **kein** `"$schema"` in die Plugin-Datei selbst: HyperLED lehnt unbekannte Felder ab. Das Schema prüft Form und Grenzen; Dinge wie „jeder Platzhalter nennt eine Einstellung“ prüft nur der Controller bei der Installation.

---

## Referenz: die Felder der Datei

Unbekannte Felder sind ein **Fehler** (damit Tippfehler auffallen). Texte sind UTF-8. Die Datei ist höchstens **16 KB** groß.

> **Längengrenzen für freien Text zählen Bytes, nicht Zeichen.** Ein ASCII-Zeichen ist ein Byte, `ä ö ü ß °` und kyrillische Buchstaben sind zwei, die meisten asiatischen Zeichen drei. Ein russisches `label` darf deshalb höchstens 30 Buchstaben haben, ein deutsches mit Umlauten entsprechend weniger als 60 Zeichen. Das JSON-Schema kann nur Zeichen zählen und ist darum bei Nicht-ASCII-Text großzügiger als der Controller; maßgeblich ist der Controller (die Vorschau meldet es).

### Oberste Ebene

| Feld | Pflicht | Beschreibung |
|---|---|---|
| `format` | ja | Immer `1`. |
| `id` | ja | Eindeutiger Name: `a-z`, `0-9`, `-`, `_`, höchstens 32 Zeichen. Eine Datei mit derselben `id` ersetzt das installierte Plugin und behält dessen Einstellungen. |
| `name` | ja | Anzeigename, höchstens 40 Bytes. |
| `version` | ja | Zum Beispiel `1.0.0`, höchstens 16 Bytes. |
| `license` | ja | Lizenzkennung (SPDX), zum Beispiel `EUPL-1.2`, höchstens 40 Bytes. |
| `author` | nein | Höchstens 60 Bytes. |
| `description` | nein | Ein, zwei Sätze, höchstens 200 Bytes. |
| `needs` | nein | Was das Plugin voraussetzt, siehe unten. |
| `settings` | nein | Liste der Einstellungen, höchstens 16. |
| `source` | ja | Woher die Werte kommen. |
| `values` | nein | Benannte Ausdrücke über die Antwort, höchstens 16. |
| `rules` | nein | Liste der Regeln, höchstens 12. |
| `on_error` | nein | Was bei fehlender Verbindung angezeigt wird. |
| `script` | nein | Ein Lua-Skript, höchstens 8 KB. Braucht `needs.script`. |

Ein Plugin, das ein Segment steuert (also `rules`, `on_error` oder `script` hat), braucht genau **eine** Einstellung vom Typ `segment`. Ein Plugin ohne diese drei liefert nur **Werte** und braucht keine (siehe „Werte als Platzhalter in Text-Elementen“ unten).

### `needs`: Verträglichkeit

| Feld | Beschreibung |
|---|---|
| `api` | Die **Plugin-Schnittstellenstufe**, für die das Plugin geschrieben ist (Standard `1`). Die Stufe steigt nur, wenn sich etwas ändert, das bestehende Plugins brechen würde. Ein Plugin mit höherer Stufe als die Firmware wird stillgelegt („Nicht kompatibel“); es bleibt gespeichert und läuft nach einem Firmware-Update von selbst. |
| `firmware` | Älteste Firmware-Version, mit der das Plugin läuft, zum Beispiel `"0.2.004"`. |
| `script` | Die **Skript-Stufe**, die das Plugin braucht. `1` bei einem Plugin mit `script`, sonst weglassen. |

Diese Firmware bietet Schnittstellenstufe **1** (älteste noch verstandene: 1) und Skript-Stufe **1**.

### `settings`: was die Person einträgt

```json
{ "key": "port", "type": "number", "label": { "de": "Port", "en": "Port" }, "hint": "Meist 7125",
  "default": 7125, "min": 1, "max": 65535, "optional": false }
```

| Feld | Pflicht | Beschreibung |
|---|---|---|
| `key` | ja | Name der Einstellung: `a-z`, `0-9`, `_`, höchstens 24 Zeichen, eindeutig. Unter diesem Namen ist sie in Adressen (`{key}`) und Ausdrücken benutzbar. |
| `type` | ja | Siehe Tabelle unten. |
| `label` | ja | Beschriftung, höchstens 60 Bytes: Text oder pro Sprache `{ "de": …, "en": …, "ru": … }`. Fehlt eine Sprache, gilt Deutsch. |
| `hint` | nein | Eine Zeile Hilfe unter dem Feld, höchstens 200 Bytes, gleiche Form wie `label`. |
| `default` | nein | Startwert; muss zum Typ passen. |
| `optional` | nein | `true`: darf leer bleiben. Alle anderen müssen ausgefüllt sein, bevor sich das Plugin einschalten lässt (Schalter ausgenommen). |
| `min`, `max` | nein | Grenzen für Zahlen. |
| `options` | bei `list` | 1 bis 20 Auswahlmöglichkeiten: Texte (`["a", "b"]`) **oder** Objekte `{ "value": "a", "label": {…} }` (nicht gemischt). `value` höchstens 60 Zeichen. |

**Typen:**

| `type` | Die Person sieht | Wert in Ausdrücken | Prüfung |
|---|---|---|---|
| `text` | Textfeld | Text | höchstens 120 Bytes |
| `password` | Passwortfeld (Wert nie angezeigt) | Text | höchstens 120 Bytes |
| `number` | Zahlfeld | Zahl (leer: unbekannt) | Zahl, innerhalb `min`/`max` |
| `switch` | Schalter | wahr/falsch | – |
| `list` | Auswahl | Text (der `value`) | einer der `options` |
| `color` | Farbwähler | Text `#rrggbb` | gültige Farbe |
| `effect` | Effektliste | Text (Effektname) | ein erlaubter Effekt |
| `segment` | Segmentliste | Text (Nummer, ab 0) | vorhandenes Segment |

`password` gehört für Zugangsdaten: Der Wert wird nie angezeigt oder an die Oberfläche geschickt. Er darf in Adressen und Kopfzeilen stehen (zum Beispiel `"header": { "X-Api-Key": "{apikey}" }`).

### `source`: woher die Werte kommen

```json
"source": {
  "url": "http://{host}:{port}/printer/objects/query?print_stats",
  "every": 5,
  "timeout": 5,
  "header": { "X-Api-Key": "{apikey}" }
}
```

| Feld | Beschreibung |
|---|---|
| `url` | Pflicht. `http://` oder `https://`, höchstens 300 Bytes. `{key}` wird durch den Wert der Einstellung ersetzt (so, wie er eingetragen ist: Sonderzeichen werden **nicht** kodiert). Es wird immer **GET** gesendet. |
| `every` | Sekunden zwischen zwei Abfragen: 2 bis 3600, Standard 5. Wähle so selten wie sinnvoll: Gemeinsame Quellen (zum Beispiel Wetterdienste) danken es dir. Nach einem Fehler wartet HyperLED mindestens 5 Sekunden. |
| `timeout` | Sekunden, die auf eine Antwort gewartet wird: 1 bis 10, Standard 5. |
| `header` | Höchstens 4 zusätzliche Kopfzeilen; die Werte dürfen `{key}` enthalten. Ist ein Wert nach dem Ersetzen leer (zum Beispiel ein optionaler Schlüssel), wird die Kopfzeile weggelassen. |

Die **Antwort** darf höchstens **8 KB** groß sein und muss JSON sein. Weiterleitungen werden befolgt. Bei `https://` wird das Zertifikat **nicht geprüft** (die Verbindung ist verschlüsselt, aber nicht beglaubigt), wie bei den anderen Verbindungen des Controllers. Die Quellen werden nacheinander abgefragt, nie gleichzeitig.

### `values`: Werte aus der Antwort

```json
"values": {
  "state":    "result.status.print_stats.state",
  "progress": "result.status.display_status.progress * 100"
}
```

Der Name links ist von dir frei gewählt (`a-z`, `0-9`, `_`, höchstens 24 Zeichen; keine Wörter der Ausdruckssprache wie `min`, `max`, `round`, `contains`, `true`, `false`, `and`, `or`, `not`; und kein Name einer Einstellung). Rechts steht ein **Ausdruck** (siehe unten), der **Pfade** in die Antwort benutzt:

- `a.b.c` geht in verschachtelte Objekte.
- `items[0].name` nimmt den Eintrag mit der Nummer 0 einer Liste.
- Teile eines Pfades bestehen aus Buchstaben, Ziffern und `_`. Ein Schlüssel mit Bindestrich (`"print-stats"`) lässt sich nicht ansprechen.
- Was es nicht gibt, ist **unbekannt**. Ein Pfad, der auf ein Objekt oder eine Liste zeigt, ist ebenfalls unbekannt – Werte sind Zahlen, Text oder wahr/falsch.

Speichersparend: HyperLED liest nur die Teile der Antwort, die in `values` vorkommen. Alles unterhalb einer Stelle mit `[n]` wird allerdings komplett gehalten (der Filter kann keine einzelnen Listeneinträge herauspicken) – bei großen Listen deshalb sparsam damit umgehen.

Liefert die Quelle fünfmal hintereinander **keinen einzigen** der Werte, geht das Plugin in den Zustand „Keine Verbindung“ mit dem Hinweis, dass die Quelle wohl nicht zum Plugin passt.

### Werte als Platzhalter in Text-Elementen

Jeder Wert aus `values` kann von Anwendern in einem Text- oder Lauftext-Element als `{<id>.<wert>}` gezeigt werden, zum Beispiel `{mein-plugin.temp}` ([Plugins nutzen](09_Plugins_nutzen.md) beschreibt die Seite der Anwender). Dafür musst du nichts Besonderes tun, aber:

- **Ein reines Wert-Plugin braucht kein Segment.** Hat die Datei weder `rules` noch `on_error` noch `script`, darf die Einstellung vom Typ `segment` fehlen: Das Plugin fragt seine Quelle ab und stellt die Werte bereit, mehr nicht.
- Der Platzhalter besteht aus der **Kennung des Plugins** (`a-z`, `0-9`, `-`, `_`) und dem **Namen des Wertes** (`a-z`, `0-9`, `_`, höchstens 24 Zeichen), getrennt durch einen Punkt.
- Der Wert erscheint nur, **solange das Plugin läuft** und der Wert bekannt ist; sonst steht `--`.
- Zahlen erscheinen ohne Nachkommastellen, wenn sie ganz sind, sonst mit zwei; Wahr/Falsch als `true` und `false`. **Rundung und Einheit gehören in den Wert**, nicht in den Platzhalter: `"temp_text": "round(current.temperature_2m) + ' °C'"` ergibt `21 °C`. Lass daneben den reinen Zahlenwert (`temp`) stehen, denn ein Wert, der Text ist, lässt sich in Regeln nicht mehr als Zahl vergleichen.
- Was die Quelle liefert, wird nicht vertraut: Steuerzeichen werden zu Leerzeichen, der Text wird auf 64 Bytes gekürzt, und der eingesetzte Text wird **nicht noch einmal** nach Platzhaltern durchsucht.
- Ein wörtliches `{id.wert}` im Text ist nicht möglich. Das Schema prüft Platzhalter nicht, sie stehen nicht in Plugin-Dateien.
- Werte stehen auch bei einem Plugin mit Skript zur Verfügung.

---

## Die Ausdruckssprache

Ausdrücke stehen in `values`, in `when`, in `speed`, `intensity`/`value` und überall, wo eine Zahl oder Bedingung gebraucht wird. Sie sind **absichtlich klein**: keine Schleifen, keine Zuweisungen, keine eigenen Funktionen. Ein Ausdruck wird einmal übersetzt und läuft dann genau einmal durch; er kann nicht hängen.

**Bausteine:**

| Was | Beispiel |
|---|---|
| Zahl | `42`, `0.5` |
| Text (in `'…'` oder `"…"`, ohne Sonderzeichen-Schreibweise) | `'printing'` |
| wahr / falsch | `true`, `false` (auch `wahr`, `falsch`) |
| Name | `temp` (ein Wert oder eine Einstellung; in `values`: ein Pfad in die Antwort) |
| Klammern | `(a + b) * c` |

**Operatoren**, von **schwach** (zuletzt ausgewertet) nach **stark**:

| Stufe | Operatoren |
|---|---|
| 1 | `or` / `oder` |
| 2 | `and` / `und` |
| 3 | `not` / `nicht` (bindet **schwächer** als der Vergleich: `not state == 'x'` heißt `not (state == 'x')`) |
| 4 | `==` `!=` |
| 5 | `<` `<=` `>` `>=` |
| 6 | `+` `-` |
| 7 | `*` `/` |
| 8 | `-` (Vorzeichen) |

**Funktionen:**

| Funktion | Ergebnis |
|---|---|
| `round(x)` | auf die nächste ganze Zahl gerundet (`round(-2.5)` ist `-3`) |
| `min(a, b)`, `max(a, b)` | kleinerer / größerer Wert |
| `contains(text, teil)` | wahr, wenn `text` den `teil` enthält |

**Unbekannt** ist das Wichtigste, das du wissen musst: Ein Wert, den es nicht gibt (Pfad fehlt, leere Zahl-Einstellung), ist *unbekannt*.

- Ein **Vergleich** mit einem unbekannten Wert ist **falsch** – auch `!=`. Fragst du „ist der Zustand nicht `printing`?“ (`state != 'printing'`) und der Zustand ist unbekannt, ist die Antwort *falsch*, nicht *wahr*. Brauchst du den Fall „unbekannt“, hänge ihn gesondert an.
- **Rechnen** mit einem unbekannten Wert ergibt wieder unbekannt (`temp + 1`).
- Eine Regel mit unbekanntem Ergebnis gilt als **nicht erfüllt**.
- Division durch null ist unbekannt.

**Verhalten bei Typen:** Zahlen und wahr/falsch lassen sich vergleichen (`true` ist `1`). Zahl und Text sind nie gleich, und `<`, `>` zwischen Zahl und Text ergeben falsch. Text lässt sich vergleichen (alphabetisch) und mit `+` verbinden (`'T=' + 215` ergibt `T=215`; Zahlen werden ohne Nachkommastellen geschrieben, wenn sie ganz sind, sonst mit zwei).

**Grenzen:** höchstens 200 Bytes, höchstens 16 offene Zwischenergebnisse (Verschachtelung). Ein Fehler (zum Beispiel „Der Ausdruck endet zu früh“, „')' fehlt“, „Text nicht beendet“, „Unerwartetes Zeichen '$'“) wird bei der Installation mit der Stelle gemeldet.

**Beispiele:**

```
state == 'printing' and progress > 0
contains(file, 'benchy') or progress >= 90
round(temp_now) >= target - 2
not (state == 'standby' or state == 'complete')
```

---

## Regeln und ihre Wirkung (`rules`, `on_error`)

```json
"rules": [
  { "when": "state == 'printing'", "show": { "effect": "Einfarbig", "color": "{c_print}" } },
  { "when": "state == 'error'",    "show": { "effect": "Stroboskop", "color": "#ff0000" } }
]
```

Eine Regel hat `when` (Bedingung) und `show` (Wirkung). Namen in `when`, `speed` und `intensity` müssen **Werte oder Einstellungen** sein (Pfade in die Antwort stehen nur in `values`).

`show` kann enthalten (mindestens eines):

| Feld | Wirkung |
|---|---|
| `effect` | Der Name eines Effekts (siehe Liste unten, Schreibweise egal) **oder** `"{key}"` einer Einstellung vom Typ `effect`: Dann wählt die Person den Effekt. |
| `color` | `#rrggbb` **oder** `"{key}"` einer Einstellung vom Typ `color`. |
| `speed` | Ein Ausdruck, der eine **Prozentzahl** von 0 bis 100 ergibt (wird auf die Geschwindigkeit 0–255 umgerechnet; Werte außerhalb werden begrenzt). |
| `intensity` oder `value` | Wie `speed`, für die Intensität des Effekts (`value` ist nur ein anderer Name, nicht beide zugleich). Je nach Effekt bedeutet Intensität zum Beispiel die Dichte (Funkeln), die Länge des Schweifs (Meteor) oder die Abkühlung (Feuer); manche Effekte, etwa `Einfarbig` und `Farbwisch`, ignorieren sie. |
| `power` | `"on"` oder `"off"`. Wirkt **nur**, wenn die Person es dem Plugin erlaubt hat. |

Weggelassene Felder lassen den Wert der Person unberührt. Ist der Wert eines Ausdrucks unbekannt, bleibt das Feld unberührt.

**Was es absichtlich nicht gibt:** die **Helligkeit**. Die Schieberegler der Person wirken wörtlich; ein Plugin kann sie nicht ändern.

**Einen Fortschrittsbalken gibt es als Effekt nicht.** Kein eingebauter Effekt füllt sich mit einem Wert: `intensity` und `value` steuern nur, was der gewählte Effekt damit macht (siehe oben). Ein Balken, der mit einem Wert wächst, braucht ein **Skript** (Beispiel „Fortschrittsbalken“ in [Plugin-Skripte](11_Plugin_Skripte.md)); die Regeln dienen dann als Rückfall, etwa mit einer Farbe je Zustand.

**Alles wird nur überlagert und nie gespeichert** – Entfernen, Ausschalten und Neustarten machen alles rückgängig.

**`on_error`** hat dieselbe Form wie eine Regel ohne `when`:

```json
"on_error": { "show": { "effect": "Atmen", "color": "#ffffff" } }
```

Es gilt, wenn die Quelle **dreimal hintereinander** nicht geantwortet hat. Fehlt `on_error`, wird das Segment dann freigegeben. Sobald die Quelle wieder antwortet, gelten wieder die Regeln.

### Erlaubte Effekte

`Einfarbig`, `Atmen`, `Regenbogen`, `Lauflicht`, `Feuer`, `Farbwisch`, `Scanner`, `Funkeln`, `Meteor`, `Matrix-Regen`, `Stroboskop`, `Prallen`, `Paletten-Regenbogen`, `Sinelon`, `Konfetti`, `Jonglieren`, `BPM`, `Theaterlicht Regenbogen`, `Wanderlicht`, `Farbwellen`, `Plasma`, `Kreiswellen`, `Feuer (2D)`, `Pacifica`, `Feuerwerk`, `Sternenfeld`, `Springbälle`.

Die Namen sind die deutschen, wie in der Weboberfläche gespeichert. **Gesperrt** für Plugins sind `Nur Weiß`, `Bild` und `Uhr / Text`: Sie brauchen, was nur das eigene Segment der Person enthält (Weißmodus, ein Bild, die Elemente). Die 2D-Effekte (`Plasma`, `Kreiswellen`, `Feuer (2D)`, `Pacifica`, `Feuerwerk`, `Sternenfeld`, `Springbälle`) zeigen ihre Stärke auf einer Matrix.

---

## Verträglichkeit, Versionen, Veröffentlichen

- **`version`:** Erhöhe sie bei jeder Änderung. Eine Installation mit derselben `id` **ersetzt** das alte Plugin; Einstellungen, deren Schlüssel und Typ noch passen, bleiben erhalten.
- **Einstellungen umbenennen oder entfernen** verliert den Wert bei den Anwendern. Neue Einstellungen brauchen einen sinnvollen `default` oder `optional: true`, sonst lässt sich das aktualisierte Plugin erst nach Eingabe einschalten.
- **Schnittstellenstufe:** Setze `needs.api` auf die Stufe, die du getestet hast. HyperLED hebt sie nur an, wenn bestehende Plugins brechen würden, und versteht dann weiterhin die älteren Stufen, soweit möglich (die Firmware kennt eine „älteste noch verstandene“ Stufe; ein älteres Plugin wird mit einem klaren Hinweis stillgelegt).
- **Veröffentlichen:** Leg die Datei in ein Repository (am besten mit `LICENSE`-Datei und `README`), hänge sie an ein Release und gib den Link auf die Datei weiter. Anwender installieren sie unter **Von einer Adresse laden**; HyperLED folgt der Weiterleitung, die GitHub für Release-Dateien benutzt.
- **Dateiname:** Der Name der Datei ist egal; maßgeblich ist `id`.

---

## Grenzen im Überblick

| Was | Grenze |
|---|---|
| Plugin-Datei | 16 KB |
| Skript | 8 KB |
| Einstellungen / Werte / Regeln | 16 / 16 / 12 |
| Plugins auf dem Gerät | 8 |
| `every` | 2 bis 3600 s |
| `timeout` | 1 bis 10 s |
| Kopfzeilen | 4 |
| Adresse | 300 Bytes |
| Antwort | 8 KB |
| Ausdruck | 200 Bytes, Tiefe 16 |
| Text einer Einstellung | 120 Bytes |
| `label` / `hint` | 60 / 200 Bytes |

Weitere Fehlermeldungen sind Deutsch und nennen die Stelle, zum Beispiel: `Regel 2 when: 'tmp' ist weder ein Wert noch eine Einstellung`, `Einstellung 'port': Unbekannter Typ 'datum'`, `source url: '{host}' ist keine Einstellung`.
