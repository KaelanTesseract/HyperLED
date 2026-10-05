# Plugin-Skripte (Lua)

Mit **Regeln** wählt ein Plugin einen vorhandenen Effekt, eine Farbe, Geschwindigkeit und Intensität. Wer mehr will – einen Fortschrittsbalken, ein Thermometer, eine eigene Animation, die auf Werte reagiert –, ergänzt ein **Skript**: ein kleines Programm in der Sprache **Lua 5.4**, das das Segment **Pixel für Pixel** zeichnet.

Ein Skript **ersetzt** die Regeln nicht, es kommt dazu: Solange das Skript läuft, zeichnet es das Segment. Kann es nicht laufen (zum Beispiel ein Slave ohne Skript-Unterstützung, ein Fehler im Skript), gelten die **Regeln** des Plugins als Rückfall, und die Oberfläche nennt den Grund. Ein Plugin mit Regeln *und* Skript bleibt so auch auf einem Gerät ohne Skripte nützlich.

Dieses Kapitel setzt voraus, dass du die Plugin-Datei kennst: [Plugins entwickeln](10_Plugins_entwickeln.md). Fertige Beispiele: [`beispiel-skript-thermometer.json`](../../plugins/beispiel-skript-thermometer.json).

---

## Ein Skript einbauen

In der Plugin-Datei kommt ein Feld `script` (der Lua-Text, höchstens 8 KB) dazu, und `needs.script` nennt die **Skript-Stufe** (derzeit `1`):

```json
{
  "format": 1,
  "id": "mein-skript",
  "name": "Mein Skript",
  "version": "1.0.0",
  "license": "EUPL-1.2",
  "needs": { "api": 1, "script": 1 },
  "settings": [ { "key": "segment", "type": "segment", "label": "Segment" } ],
  "source": { "url": "https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m" },
  "values": { "temp": "current.temperature_2m" },
  "script": "function frame(t)\n  fill(0, 0, 40)\nend\n"
}
```

Der Lua-Text steht als JSON-Zeichenkette, Zeilenumbrüche als `\n`. Schreibe das Skript am besten in einer eigenen `.lua`-Datei und füge es mit einem kleinen Werkzeug ein (zum Beispiel `python -c "import json; print(json.dumps(open('skript.lua').read()))"`).

`source` ist wie bei jedem Plugin Pflicht; `values` darf fehlen, dann ist `v` leer. Was die Quelle liefert, sieht das Skript als `v`.

Beim Installieren wird das Skript **übersetzt, aber nicht ausgeführt**. Ein Syntaxfehler wird mit der Zeile abgelehnt (`Skript: script:7: …`), bevor das Plugin gespeichert wird.

---

## Wie ein Skript abläuft

```lua
-- 1. Oberste Ebene: läuft einmal beim Laden.
fps = 20                 -- (optional) Bilder pro Sekunde

function init()          -- (optional) einmal, direkt nach dem Laden
end

function update()        -- (optional) jedes Mal, wenn sich settings oder v geändert haben
end

function frame(t, dt)    -- (PFLICHT) bei jedem Bild
  -- zeichnen
end
```

- **`settings` und `v` stehen erst ab `update()` bereit.** Auf der obersten Ebene und in `init()` sind sie noch leer: Das Skript wird zuerst geladen, danach bekommt es die Werte. Lies Einstellungen deshalb in `update()` (oder in `frame`), nicht beim Laden.
- `frame(t, dt)`: `t` sind die **Millisekunden seit dem ersten Bild**, `dt` die Millisekunden seit dem Bild davor. Zeichne ein ganzes Bild; HyperLED kopiert es danach auf das Segment.
- `update()`: wird aufgerufen, wenn die Quelle **neue Werte** geliefert hat (und gleich nach dem Laden, wenn schon Werte da sind). Hier lässt sich Rechenarbeit unterbringen, die nicht jedes Bild nötig ist.
- `fps`: die Bildrate, 1 bis 60, Standard 30. Du kannst sie am Anfang setzen oder später ändern. Wähle sie so klein wie möglich: Ein Balken, der sich alle paar Minuten ändert, braucht keine 30 Bilder pro Sekunde.
- **Ändert die Person die Einstellungen** des Plugins, startet das Skript neu (`init()` läuft wieder).
- Schaltet die Person das Segment aus, ruht das Skript; das Segment wird dunkel.

---

## Was das Skript sieht

### Globale Werte (nur lesen)

| Name | Bedeutung |
|---|---|
| `W`, `H` | Breite und Höhe des **Zeichenfelds** in Pixeln; `N` ist `W * H`. Ein Streifen hat `H = 1`. |
| `settings` | Die Einstellungen des Plugins als Tabelle (siehe unten). |
| `v` | Die Werte aus der Quelle als Tabelle (`values` des Plugins). Was unbekannt ist, **fehlt** (`nil`). |
| `fps` | Die Bildrate (setzbar). |

Das Zeichenfeld ist:

- bei einem **Streifen**: `W` = Anzahl der LEDs des Segments, `H` = 1;
- beim **Master im Matrix-Betrieb**: die Matrix des Masters (`W` × `H`);
- auf einem **Slave**: das Feld des Slaves: ein HUB75-Panel `W` × `H` oder ein Streifen mit seiner LED-Anzahl.

### `settings`

Die Einstellungen des Plugins, unter ihrem `key`:

| Typ | Im Skript |
|---|---|
| `number`, `segment` | Zahl |
| `switch` | `true` / `false` (auch `false`, nie `nil`) |
| `color` | **Zahl `0xRRGGBB`**, zum Beispiel `#00ff80` ist `0x00FF80` (`65408`). Zerlegen: `r = (c // 65536) % 256`, `g = (c // 256) % 256`, `b = c % 256` |
| `text`, `list`, `effect` | Text |
| `password` | **nicht vorhanden** |

Dazu kommen drei Angaben zum **Segment**, die HyperLED selbst einträgt (Namen mit Unterstrich, damit sie sich nicht mit eigenen Einstellungen beißen):

| Name | Bedeutung |
|---|---|
| `settings._leds` | Wie viele LEDs das Segment hat. |
| `settings._first` | Wo die LEDs des Segments in der Kette beginnen (die k-te LED des Segments ist LED `_first + k`). |
| `settings._layout` | `"grid"`: das Zeichenfeld **ist** das Segment (Panel, einfacher Streifen, alles auf einem Slave). `"snake"` oder `"rows"`: Der Master hat eine Matrix eingerichtet, das Segment ist aber ein **Streifen**, der darauf hängt; seine LEDs sind die ersten Pixel des Feldes, Reihe für Reihe (`"snake"`: jede zweite Reihe rückwärts, `"rows"`: jede Reihe von links). Wer einen Balken der Reihe nach über einen solchen Streifen laufen lassen will, rechnet die LED-Nummer `i` auf `x = i % W` (bei `"snake"` und ungerader Reihe `W - 1 - x`) und `y = i // W` um. |

Leere Einstellungen fehlen (`nil`). **Passwörter erhält ein Skript nie:** Es kann auf einem Slave laufen, und alles, was es bekommt, geht über Funk dorthin – unverschlüsselt.

```lua
local c = settings.farbe or 0x00FF00
local r, g, b = (c // 65536) % 256, (c // 256) % 256, c % 256
```

### `v`

Die Werte aus `values`, mit denselben Namen. Zahlen kommen als Zahl an, Text als Text, wahr/falsch als `true`/`false`. Ein unbekannter Wert **fehlt**: `v.temp` ist dann `nil`. Frage das ab, bevor du damit rechnest:

```lua
if v.temp == nil then ... end
local t = v.temp or 0
```

---

## Zeichnen

| Funktion | Wirkung |
|---|---|
| `px(x, y, r, g, b)` | Ein Pixel. `x` von 0 bis `W-1`, `y` von 0 bis `H-1`, Farben 0 bis 255 (größere Werte werden begrenzt). Koordinaten **außerhalb** werden ignoriert. Kommazahlen werden abgerundet. |
| `fill(r, g, b)` | Alle Pixel in einer Farbe. |
| `clear()` | Alles schwarz. |
| `hsv(h, s, v)` | Rechnet eine Farbe um und gibt `r, g, b` zurück. `h` 0 bis 359 (jede ganze Zahl, sie läuft rund), `s` und `v` 0 bis 255. |
| `log(text)` | Eine Meldung für dich als Autor (siehe unten). |

Das Skript zeichnet immer mit **voller Helligkeit** (0 bis 255). Die **Helligkeit** der Person, die Strombegrenzung und bei HUB75-Panels die Treiber-Helligkeit werden danach von HyperLED angewendet. Zeichne also nicht „dunkel“, um zu dimmen.

Das Bild bleibt stehen, bis das nächste fertig ist: Ruft `frame` nichts auf, bleibt das vorige Bild. Ein Bild, das mit einem Fehler abbricht, wird übersprungen.

---

## Beispiele

### Fortschrittsbalken

Ein Balken von links, der sich mit einem Wert `progress` (0 bis 100) füllt, in der Farbe aus den Einstellungen:

```lua
function frame(t)
  local share = (v.progress or 0) / 100
  local filled = math.floor(share * W + 0.5)
  local c = settings.farbe or 0x00FF00
  local r, g, b = (c // 65536) % 256, (c // 256) % 256, c % 256
  for y = 0, H - 1 do
    for x = 0, W - 1 do
      if x < filled then px(x, y, r, g, b) else px(x, y, 0, 0, 0) end
    end
  end
end
```

### Regenbogen, der wandert

`t` bringt die Bewegung:

```lua
function frame(t)
  for x = 0, W - 1 do
    local r, g, b = hsv((x * 360 // W + t // 20) % 360, 255, 255)
    for y = 0, H - 1 do
      px(x, y, r, g, b)
    end
  end
end
```

### Warnblinken bei einem Zustand

`update()` rechnet nur, wenn neue Werte da sind; `frame` blinkt dann:

```lua
local windy = false

function update()  -- neue Werte sind da
  windy = (v.wind or 0) > 20
end

function frame(t)
  if windy and (t // 500) % 2 == 0 then
    fill(255, 120, 0)
  else
    fill(0, 40, 0)
  end
end
```

### Thermometer

Das vollständige Beispiel mit Skala aus den Einstellungen steht in [`plugins/beispiel-skript-thermometer.json`](../../plugins/beispiel-skript-thermometer.json): Die Länge des Balkens folgt der Temperatur, die Farbe geht von Blau über Grün nach Rot (`hsv(240 - share * 240, …)`).

---

## Sprache und Bibliotheken

Es gilt Lua 5.4 – mit **Einschränkungen**, damit ein Skript nichts außer seinem Segment erreichen kann:

| Verfügbar | Nicht verfügbar |
|---|---|
| Grundfunktionen (`type`, `tostring`, `tonumber`, `ipairs`, `pairs`, `select`, `setmetatable`, `error`, `assert` …), `math`, `string`, `table` | `os`, `io`, `package`, `debug`, `coroutine`, `utf8`; `require`, `dofile`, `loadfile`, `load`, `collectgarbage`, `warn`, `pcall`, `xpcall`; `string.dump` |

`print` ist dasselbe wie `log`. **Nur Text** wird geladen, kein vorübersetzter Code. `error("…")` bricht den Aufruf ab (siehe Fehler).

### Zahlen: 32 Bit

HyperLED benutzt Lua mit **32-Bit-Zahlen**: Ganzzahlen von etwa ±2 Milliarden, und Kommazahlen mit **einfacher Genauigkeit** (etwa 7 gültige Ziffern). Das ist schnell und reicht für Pixel. Folgen:

- Sehr große Zahlen und sehr genaue Rechnungen gehen nicht.
- `t` ist eine ganze Zahl in Millisekunden und **läuft nach etwa 24 Tagen** über und beginnt wieder bei 0. Rechne nicht mit sehr langen Zeiträumen.
- `/` liefert immer eine Kommazahl, `//` eine abgerundete Ganzzahl. Für Pixelnummern `math.floor` oder `//` benutzen.
- Zahlen aus der Quelle kommen als Kommazahlen mit einfacher Genauigkeit an (`60` kann als `60.00000238` ankommen). Runde oder vergleiche mit Spielraum (`math.abs(a - b) < 0.01`).

Es gibt keine Uhr und kein Datum; die Zeit kommt nur aus `t`. Zufall: `math.random`.

---

## Grenzen

| Was | Grenze |
|---|---|
| Skripttext | 8 KB |
| Zeit je Aufruf (`frame`, `update`, `init`) | **40 ms**; bei Feldern über 1024 Pixeln (zum Beispiel ein 64×64-Panel) **120 ms**, auf dem Master wie auf einem Slave |
| Arbeitsspeicher des Skripts | 48 KB auf dem Master, **32 KB** auf einem Slave (alles zusammen, was das Skript anlegt) |
| Bildrate (`fps`) | 1 bis 60 |
| Verschachtelungstiefe (Klammern, Blöcke, Funktionen, Metamethoden) | 32 Ebenen; tiefer ergibt „C stack overflow“ |
| Muster in `string.find` & Co. | höchstens 100 Ebenen Rekursion |

Ein Aufruf, der länger als sein Zeitbudget braucht, wird **abgebrochen** (Fehler „Zeitbudget ueberschritten“). Das fängt auch Endlosschleifen ab (`while true do end`, Endlosrekursion, Muster, die ewig suchen). Wer den Speicher überschreitet, bekommt „not enough memory“.

### Wie schnell ist es?

Gemessen auf dem ESP32-S3 mit einem 64×64-Feld (4096 Pixel):

| Skript | Zeit je Bild |
|---|---|
| `fill(...)` | etwa 1 ms |
| Alle Pixel mit `px` in einer Farbe zeichnen | 18–19 ms |
| Alle Pixel mit Verlauf (`px(x, y, x*4, y*4, b)`) | 20 ms |
| Alle Pixel mit `math.sin` (Plasma) | 64–85 ms auf dem Master, 68–75 ms auf dem Slave (siehe den Hinweis zu `sin` unten) |

Daraus folgt: Einfache bis mittlere Skripte schaffen auf einem 64×64-Panel **30 Bilder pro Sekunde** (`fps = 30`) oder mehr, rechenintensive nur noch etwa 12 bis 15. Ein Plasma über das ganze Panel liegt bei 70–85 ms je Bild und passt in das Budget von 120 ms – es bleibt aber nicht viel Luft: Ein einzelnes Bild, das einmal länger dauert (weil der Funk gerade Rechenzeit braucht), wird abgebrochen; erst **drei Abbrüche hintereinander** beenden das Skript. Auf Streifen kostet Lua praktisch nichts.

**`sin` und `cos` mit kleinen Argumenten füttern.** Auf dem ESP32-S3 wird `math.sin` merklich langsamer, wenn das Argument größer als etwa 128 ist (gemessen: ein 64×64-Plasma braucht mit Argumenten um 0 bis 100 rund 82 ms, ab etwa 200 mehr als 120 ms). Wer `t` ungeprüft in `sin(x / 8 + t * 0.001)` steckt, hat nach zweieinhalb Minuten ein Skript, das am Zeitbudget scheitert. Reduziere die Zeit zuerst, so dass der Wert eine volle Drehung (`2π ≈ 6,283`) macht und wieder bei 0 beginnt – ein Vielfaches davon macht keinen Sprung:

```lua
local sin, floor = math.sin, math.floor
fps = 15

function frame(t)
  local a = (t % 6283) * 0.001   -- one turn every 6.283 s: sin() stays fast, and there is no jump
  for y = 0, H - 1 do
    for x = 0, W - 1 do
      local s = sin(x / 8 + a) + sin(y / 6 + 2 * a) + sin((x + y) / 10 - a)
      local c = floor((s + 3) * 42.5)
      px(x, y, c, 255 - c, 128)
    end
  end
end
```

Die ganzen Vielfachen der Drehung (`a`, `2 * a`, `-a`) halten die Animation stetig. Dieses Plasma lief fünf Minuten am Stück auf einem 64×64-Panel mit 72,8 ms je Bild.

Tipps für schnelle Skripte:

- Zeichne **nur, was sich ändert**: ein `fill` und dann ein paar `px` ist viel billiger als alle Pixel zu setzen.
- Rechne einmal in `update()` oder auf der obersten Ebene vor (Tabellen, Farben), nicht in jedem Bild.
- Hole Funktionen in lokale Variablen (`local sin = math.sin`) – schneller als der Zugriff über `math.`.
- Wähle eine kleine `fps`.
- Lege im Bild keine neuen Tabellen oder Texte an (Speicher und Aufräumarbeit).

---

## Fehler und Fehlersuche

- **Syntaxfehler** meldet die Installation mit der Zeile: `Skript: script:7: 'end' expected near '<eof>'`.
- **Laufzeitfehler** (`error(...)`, Rechnen mit `nil`, falscher Typ) brechen den Aufruf ab; die Meldung steht im Plugin unter **Skript** (mit `script:ZEILE:`) und bei **Live-Werte**. Das vorige Bild bleibt.
- **Drei fehlgeschlagene Aufrufe hintereinander** (oder zehn in einer Minute) beenden das Skript. Es wird danach **nicht** erneut gestartet, bis die Person das Plugin aus- und wieder einschaltet oder die Einstellungen ändert. Bis dahin gelten die **Regeln** des Plugins als Rückfall.
- **`log(text)`** schreibt eine Meldung für dich: höchstens **eine pro Sekunde** und **80 Zeichen**. Sie erscheint als „letzte Meldung“ im Skriptzustand bei **Live-Werte**, auch solange das Skript fehlerfrei läuft. Sie ist zum Lesen gedacht, nicht als Ausgabe: `log(v.temp)` zeigt dir, was ankommt.
- Die Zeile **„Skript läuft · 20,5 ms je Bild“** in der Plugin-Liste zeigt die durchschnittliche Zeit eines Bildes. Liegt sie nahe am Zeitbudget, wird es knapp.

---

## Wo läuft das Skript?

- Gehört das Segment dem **Master**, läuft das Skript auf dem Master (in einer eigenen Aufgabe, die den Rest nie aufhält).
- Gehört es einem **Slave**, schickt der Master Skript und Werte hin, und der Slave führt es **selbst** aus. Nur die Werte gehen über die Funkstrecke, nie Pixel. Das setzt **Slave-Firmware 0.3.000 oder neuer** voraus; ein älterer Slave wird erkannt, und das Plugin zeigt, was seine Regeln vorsehen.

Pro Segment läuft **ein** Skript. Nach einem Neustart des Masters sendet das Plugin das Skript von selbst neu.

Für `needs.script`: Skript-Stufe **1** ist diese Schnittstelle. Spätere Erweiterungen (zum Beispiel Uhrzeit oder Text) erhöhen die Stufe, nicht das Dateiformat. Ein Plugin mit einer höheren Stufe wird auf älteren Geräten stillgelegt und läuft nach einem Update von selbst.
