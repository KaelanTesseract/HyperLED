# Developing Plugins

A plugin is **one JSON file**. It describes four things:

1. **Settings** (`settings`): what the person fills in, for example an address or a colour.
2. **A source** (`source`): which address HyperLED queries.
3. **Values** (`values`): which parts of the answer matter.
4. **Rules** (`rules`): what is shown on the segment, depending on the values.

If you need more than effect, colour and speed, add a **script** (Lua, see [Plugin Scripts](11_Plugin_Skripte.md)). Everything in this chapter still applies then.

You need **no software except a text editor** and a HyperLED on the network: installation checks the file at once and reports errors with the place. A JSON schema for editors is in [`plugins/plugin.schema.json`](../../plugins/plugin.schema.json).

---

## Your first plugin in five steps

The examples in the [`plugins/`](../../plugins/) folder query the free weather interface [Open-Meteo](https://open-meteo.com/) (no registration, for private use); you can try them right away.

### 1. The skeleton

```json
{
  "format": 1,
  "id": "my-plugin",
  "name": "My first plugin",
  "version": "1.0.0",
  "license": "EUPL-1.2",
  "source": { "url": "https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m" }
}
```

`format` is always `1`. `id` is the unique name (only `a-z`, `0-9`, `-`, `_`). `license` is required: say under which terms your plugin may be passed on. The source is an address that HyperLED queries with **GET** and that delivers JSON.

### 2. Reading values from the answer

The answer of the source looks like this:

```json
{ "current": { "time": "2026-10-04T15:30", "temperature_2m": 19.4 } }
```

`values` gives names to what you need. The expression on the right is the **path** into the answer:

```json
"values": { "temp": "current.temperature_2m" }
```

### 3. Rules: what should be shown?

```json
"rules": [
  { "when": "temp > 25",  "show": { "effect": "Einfarbig", "color": "#ff3300" } },
  { "when": "temp <= 25", "show": { "effect": "Einfarbig", "color": "#0066ff" } }
]
```

The rules are checked **from top to bottom**; the **first** whose `when` holds applies. If none fits, the segment stays the way the person set it.

### 4. The segment: one setting

The plugin shows something on a segment, so it needs a setting of type `segment` in which the person chooses the segment:

```json
"settings": [
  { "key": "segment", "type": "segment", "label": { "de": "Segment", "en": "Segment" } }
]
```

Done: this is [`beispiel-minimal.json`](../../plugins/beispiel-minimal.json). Install it under **Settings → Plugins**, choose a segment and switch it on.

### 5. Settings for everything the person should be able to change

Instead of writing `52.52` and `13.41` into the address, let the person enter their location:

```json
"settings": [
  { "key": "lat", "type": "number", "default": 52.52, "min": -90, "max": 90, "label": "Latitude" },
  { "key": "lon", "type": "number", "default": 13.41, "min": -180, "max": 180, "label": "Longitude" },
  { "key": "segment", "type": "segment", "label": "Segment" }
],
"source": { "url": "https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m" }
```

`{lat}` in the address is replaced by the value of the setting `lat`. Thresholds and colours become settings in the same way; in the rules you use them like values (`temp <= cold`) or with curly braces (`"color": "{c_warm}"`). The complete example with thresholds, colours, a selectable effect and an error state is [`beispiel-wetter.json`](../../plugins/beispiel-wetter.json).

---

## Checking and trying out

- **Preview without saving:** `POST /api/plugins/preview` checks a file completely and saves nothing:

  ```bash
  curl -s -X POST -H "Content-Type: application/json" --data-binary @my-plugin.json http://<IP-of-the-controller>/api/plugins/preview
  ```

  On success you get name, address and compatibility back, on an error `{"error": "…"}` with the place.
- **Live values:** in the interface, **Live values** shows what the plugin reads, which rule applies right now and the beginning of the answer - without changing a segment. `GET /api/plugins/values?id=<id>` returns the same.
- **Look at your own source:** call the address with `curl` and look at the answer. Paths that are not there give "unknown".
- **Editor:** the [JSON schema](../../plugins/plugin.schema.json) gives you completion and warnings. In Visual Studio Code, for example, in the settings under `json.schemas`:

  ```json
  "json.schemas": [ { "fileMatch": ["**/*.plugin.json"], "url": "./plugins/plugin.schema.json" } ]
  ```

  Do **not** put a `"$schema"` into the plugin file itself: HyperLED rejects unknown fields. The schema checks shape and limits; things like "every placeholder names a setting" are only checked by the controller on installation.

---

## Reference: the fields of the file

Unknown fields are an **error** (so that typos stand out). Texts are UTF-8. The file is at most **16 KB**.

### Top level

| Field | Required | Description |
|---|---|---|
| `format` | yes | Always `1`. |
| `id` | yes | Unique name: `a-z`, `0-9`, `-`, `_`, at most 32 characters. A file with the same `id` replaces the installed plugin and keeps its settings. |
| `name` | yes | Display name, at most 40 characters. |
| `version` | yes | For example `1.0.0`, at most 16 characters. |
| `license` | yes | Licence identifier (SPDX), for example `EUPL-1.2`, at most 40 characters. |
| `author` | no | At most 60 characters. |
| `description` | no | One or two sentences, at most 200 characters. |
| `needs` | no | What the plugin requires, see below. |
| `settings` | no | List of settings, at most 16. |
| `source` | yes | Where the values come from. |
| `values` | no | Named expressions over the answer, at most 16. |
| `rules` | no | List of rules, at most 12. |
| `on_error` | no | What to show when there is no connection. |
| `script` | no | A Lua script, at most 8 KB. Needs `needs.script`. |

A plugin that controls a segment (that is, has `rules`, `on_error` or `script`) needs exactly **one** setting of type `segment`.

### `needs`: compatibility

| Field | Description |
|---|---|
| `api` | The **plugin interface level** the plugin was written for (default `1`). The level only rises when something changes that would break existing plugins. A plugin with a higher level than the firmware is shut down ("Not compatible"); it stays stored and runs by itself after a firmware update. |
| `firmware` | Oldest firmware version the plugin runs with, for example `"0.2.004"`. |
| `script` | The **script level** the plugin needs. `1` for a plugin with `script`, otherwise leave it out. |

This firmware offers interface level **1** (oldest still understood: 1) and script level **1**.

### `settings`: what the person enters

```json
{ "key": "port", "type": "number", "label": { "de": "Port", "en": "Port" }, "hint": "Usually 7125",
  "default": 7125, "min": 1, "max": 65535, "optional": false }
```

| Field | Required | Description |
|---|---|---|
| `key` | yes | Name of the setting: `a-z`, `0-9`, `_`, at most 24 characters, unique. Under this name it can be used in addresses (`{key}`) and expressions. |
| `type` | yes | See the table below. |
| `label` | yes | Label, at most 60 characters: text or per language `{ "de": …, "en": …, "ru": … }`. A missing language falls back to German. |
| `hint` | no | One line of help under the field, at most 200 characters, same form as `label`. |
| `default` | no | Starting value; must fit the type. |
| `optional` | no | `true`: may stay empty. All others must be filled in before the plugin can be switched on (switches excepted). |
| `min`, `max` | no | Limits for numbers. |
| `options` | for `list` | 1 to 20 choices: texts (`["a", "b"]`) **or** objects `{ "value": "a", "label": {…} }` (not mixed). `value` at most 60 characters. |

**Types:**

| `type` | The person sees | Value in expressions | Check |
|---|---|---|---|
| `text` | text field | text | at most 120 characters |
| `password` | password field (value never shown) | text | at most 120 characters |
| `number` | number field | number (empty: unknown) | a number, within `min`/`max` |
| `switch` | switch | true/false | - |
| `list` | choice | text (the `value`) | one of the `options` |
| `color` | colour picker | text `#rrggbb` | a valid colour |
| `effect` | effect list | text (effect name) | an allowed effect |
| `segment` | segment list | text (number, from 0) | an existing segment |

Use `password` for credentials: the value is never shown or sent to the interface. It may appear in addresses and headers (for example `"header": { "X-Api-Key": "{apikey}" }`).

### `source`: where the values come from

```json
"source": {
  "url": "http://{host}:{port}/printer/objects/query?print_stats",
  "every": 5,
  "timeout": 5,
  "header": { "X-Api-Key": "{apikey}" }
}
```

| Field | Description |
|---|---|
| `url` | Required. `http://` or `https://`, at most 300 characters. `{key}` is replaced by the value of the setting (as entered: special characters are **not** encoded). Always sent as **GET**. |
| `every` | Seconds between two requests: 2 to 3600, default 5. Choose as rarely as sensible: shared sources (for example weather services) will thank you. After an error HyperLED waits at least 5 seconds. |
| `timeout` | Seconds to wait for an answer: 1 to 10, default 5. |
| `header` | At most 4 extra headers; the values may contain `{key}`. If a value is empty after replacing (for example an optional key), the header is left out. |

The **answer** may be at most **8 KB** and must be JSON. Redirects are followed. With `https://` the certificate is **not checked** (the connection is encrypted but not authenticated), like the controller's other connections. Sources are queried one after the other, never at the same time.

### `values`: values from the answer

```json
"values": {
  "state":    "result.status.print_stats.state",
  "progress": "result.status.display_status.progress * 100"
}
```

The name on the left is yours to choose (`a-z`, `0-9`, `_`, at most 24 characters; none of the words of the expression language such as `min`, `max`, `round`, `contains`, `true`, `false`, `and`, `or`, `not`; and not the name of a setting). On the right is an **expression** (see below) that uses **paths** into the answer:

- `a.b.c` goes into nested objects.
- `items[0].name` takes the entry with number 0 of a list.
- Parts of a path consist of letters, digits and `_`. A key with a hyphen (`"print-stats"`) cannot be addressed.
- Whatever is not there is **unknown**. A path that points to an object or a list is unknown too - values are numbers, text or true/false.

Memory-saving: HyperLED reads only the parts of the answer that occur in `values`. Everything below a place with `[n]` is kept whole, though (the filter cannot pick single list entries) - use it sparingly with big lists.

If the source delivers **none** of the values five times in a row, the plugin goes to the "No connection" state with the hint that the source probably does not fit the plugin.

---

## The expression language

Expressions appear in `values`, in `when`, in `speed`, `intensity`/`value` and wherever a number or condition is needed. They are **deliberately small**: no loops, no assignments, no functions of your own. An expression is compiled once and then runs through exactly once; it cannot hang.

**Building blocks:**

| What | Example |
|---|---|
| Number | `42`, `0.5` |
| Text (in `'…'` or `"…"`, no escape sequences) | `'printing'` |
| true / false | `true`, `false` (also `wahr`, `falsch`) |
| Name | `temp` (a value or a setting; in `values`: a path into the answer) |
| Parentheses | `(a + b) * c` |

**Operators**, from **weak** (evaluated last) to **strong**:

| Level | Operators |
|---|---|
| 1 | `or` / `oder` |
| 2 | `and` / `und` |
| 3 | `not` / `nicht` (binds **looser** than comparison: `not state == 'x'` means `not (state == 'x')`) |
| 4 | `==` `!=` |
| 5 | `<` `<=` `>` `>=` |
| 6 | `+` `-` |
| 7 | `*` `/` |
| 8 | `-` (sign) |

**Functions:**

| Function | Result |
|---|---|
| `round(x)` | rounded to the nearest whole number (`round(-2.5)` is `-3`) |
| `min(a, b)`, `max(a, b)` | smaller / larger value |
| `contains(text, part)` | true if `text` contains `part` |

**Unknown** is the most important thing to know: a value that does not exist (path missing, empty number setting) is *unknown*.

- A **comparison** with an unknown value is **false** - `!=` too. If you ask "is the state not `printing`?" (`state != 'printing'`) and the state is unknown, the answer is *false*, not *true*. If you need the "unknown" case, add it separately.
- **Calculating** with an unknown value gives unknown again (`temp + 1`).
- A rule whose result is unknown counts as **not fulfilled**.
- Division by zero is unknown.

**Behaviour of types:** numbers and true/false can be compared (`true` is `1`). A number and a text are never equal, and `<`, `>` between a number and a text give false. Text can be compared (alphabetically) and joined with `+` (`'T=' + 215` gives `T=215`; numbers are written without decimals when they are whole, otherwise with two).

**Limits:** at most 200 characters, at most 16 pending intermediate results (nesting). An error (for example "Der Ausdruck endet zu früh", "')' fehlt", "Text nicht beendet", "Unerwartetes Zeichen '$'") is reported on installation with the place.

**Examples:**

```
state == 'printing' and progress > 0
contains(file, 'benchy') or progress >= 90
round(temp_now) >= target - 2
not (state == 'standby' or state == 'complete')
```

---

## Rules and their effect (`rules`, `on_error`)

```json
"rules": [
  { "when": "state == 'printing'", "show": { "effect": "Farbwisch", "color": "{c_print}", "value": "progress" } },
  { "when": "state == 'error'",    "show": { "effect": "Stroboskop", "color": "#ff0000" } }
]
```

A rule has `when` (condition) and `show` (effect). Names in `when`, `speed` and `intensity` must be **values or settings** (paths into the answer appear only in `values`).

`show` can contain (at least one):

| Field | Effect |
|---|---|
| `effect` | The name of an effect (see the list below, case does not matter) **or** `"{key}"` of a setting of type `effect`: then the person chooses the effect. |
| `color` | `#rrggbb` **or** `"{key}"` of a setting of type `color`. |
| `speed` | An expression that gives a **percentage** from 0 to 100 (converted to the speed 0-255; values outside are limited). |
| `intensity` or `value` | Like `speed`, for the intensity of the effect (`value` is just another name, not both at once). Depending on the effect, intensity means for example density, length or fill level. |
| `power` | `"on"` or `"off"`. Works **only** if the person has allowed it for the plugin. |

Omitted fields leave the person's value untouched. If the value of an expression is unknown, the field stays untouched.

**What deliberately does not exist:** **brightness**. The person's sliders act literally; a plugin cannot change them.

**Everything is only laid over and never stored** - removing, switching off and restarting undo everything.

**`on_error`** has the same form as a rule without `when`:

```json
"on_error": { "show": { "effect": "Atmen", "color": "#ffffff" } }
```

It applies when the source has **not answered three times in a row**. If `on_error` is missing, the segment is released then. As soon as the source answers again, the rules apply again.

### Allowed effects

`Einfarbig`, `Atmen`, `Regenbogen`, `Lauflicht`, `Feuer`, `Farbwisch`, `Scanner`, `Funkeln`, `Meteor`, `Matrix-Regen`, `Stroboskop`, `Prallen`, `Paletten-Regenbogen`, `Sinelon`, `Konfetti`, `Jonglieren`, `BPM`, `Theaterlicht Regenbogen`, `Wanderlicht`, `Farbwellen`, `Plasma`, `Kreiswellen`, `Feuer (2D)`, `Pacifica`, `Feuerwerk`, `Sternenfeld`, `Springbälle`.

The names are the German ones, as stored by the web interface. **Blocked** for plugins are `Nur Weiß`, `Bild` and `Uhr / Text`: they need what only the person's own segment contains (white mode, an image, the elements). The 2D effects (`Plasma`, `Kreiswellen`, `Feuer (2D)`, `Pacifica`, `Feuerwerk`, `Sternenfeld`, `Springbälle`) show their strength on a matrix.

---

## Compatibility, versions, publishing

- **`version`:** raise it with every change. An installation with the same `id` **replaces** the old plugin; settings whose key and type still fit are kept.
- **Renaming or removing settings** loses the value for the users. New settings need a sensible `default` or `optional: true`, otherwise the updated plugin can only be switched on after entering something.
- **Interface level:** set `needs.api` to the level you tested. HyperLED raises it only if existing plugins would break, and then keeps understanding the older levels as far as possible (the firmware knows an "oldest still understood" level; an older plugin is shut down with a clear hint).
- **Publishing:** put the file into a repository (ideally with a `LICENSE` file and a `README`), attach it to a release and pass on the link to the file. Users install it with **Load from an address**; HyperLED follows the redirect GitHub uses for release files.
- **File name:** the name of the file does not matter; `id` is what counts.

---

## Limits at a glance

| What | Limit |
|---|---|
| Plugin file | 16 KB |
| Script | 8 KB |
| Settings / values / rules | 16 / 16 / 12 |
| Plugins on the device | 8 |
| `every` | 2 to 3600 s |
| `timeout` | 1 to 10 s |
| Headers | 4 |
| Address | 300 characters |
| Answer | 8 KB |
| Expression | 200 characters, depth 16 |
| Text of a setting | 120 characters |
| `label` / `hint` | 60 / 200 characters |

Other error messages are in German and name the place, for example: `Regel 2 when: 'tmp' ist weder ein Wert noch eine Einstellung` (rule 2 when: 'tmp' is neither a value nor a setting), `Einstellung 'port': Unbekannter Typ 'datum'` (setting 'port': unknown type), `source url: '{host}' ist keine Einstellung` ('{host}' is not a setting).
