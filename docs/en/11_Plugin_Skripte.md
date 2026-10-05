# Plugin Scripts (Lua)

With **rules** a plugin picks an existing effect, a colour, speed and intensity. If you want more - a progress bar, a thermometer, an animation of your own that reacts to values - add a **script**: a small program in the language **Lua 5.4** that draws the segment **pixel by pixel**.

A script does **not replace** the rules, it comes in addition: as long as the script runs, it draws the segment. If it cannot run (for example a Slave without script support, an error in the script), the plugin's **rules** apply as a fallback, and the interface names the reason. A plugin with rules *and* a script stays useful on a device without scripts as well.

This chapter assumes you know the plugin file: [Developing Plugins](10_Plugins_entwickeln.md). A ready-made example: [`beispiel-skript-thermometer.json`](../../plugins/beispiel-skript-thermometer.json).

---

## Adding a script

The plugin file gets a field `script` (the Lua text, at most 8 KB), and `needs.script` names the **script level** (currently `1`):

```json
{
  "format": 1,
  "id": "my-script",
  "name": "My script",
  "version": "1.0.0",
  "license": "EUPL-1.2",
  "needs": { "api": 1, "script": 1 },
  "settings": [ { "key": "segment", "type": "segment", "label": "Segment" } ],
  "source": { "url": "https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m" },
  "values": { "temp": "current.temperature_2m" },
  "script": "function frame(t)\n  fill(0, 0, 40)\nend\n"
}
```

The Lua text is a JSON string, line breaks written as `\n`. It is best to write the script in a separate `.lua` file and insert it with a small tool (for example `python -c "import json; print(json.dumps(open('script.lua').read()))"`).

`source` is required as for every plugin; `values` may be missing, then `v` is empty. What the source delivers, the script sees as `v`.

On installation the script is **compiled but not run**. A syntax error is refused with its line (`Skript: script:7: …`) before the plugin is saved.

---

## How a script runs

```lua
-- 1. Top level: runs once when loaded.
fps = 20                 -- (optional) frames per second

function init()          -- (optional) once, right after loading
end

function update()        -- (optional) every time settings or v have changed
end

function frame(t, dt)    -- (REQUIRED) on every frame
  -- draw
end
```

- **`settings` and `v` are only there from `update()` on.** At the top level and in `init()` they are still empty: the script is loaded first and gets the values afterwards. So read settings in `update()` (or in `frame`), not while loading.
- `frame(t, dt)`: `t` is the **milliseconds since the first frame**, `dt` the milliseconds since the frame before. Draw a whole frame; HyperLED copies it to the segment afterwards.
- `update()`: called when the source has delivered **new values** (and right after loading if values are already there). A good place for work that is not needed on every frame.
- `fps`: the frame rate, 1 to 60, default 30. You can set it at the start or change it later. Choose it as small as possible: a bar that changes every few minutes does not need 30 frames per second.
- **If the person changes the plugin's settings**, the script restarts (`init()` runs again).
- If the person switches the segment off, the script rests; the segment goes dark.

---

## What the script sees

### Global values (read only)

| Name | Meaning |
|---|---|
| `W`, `H` | Width and height of the **drawing area** in pixels; `N` is `W * H`. A strip has `H = 1`. |
| `settings` | The plugin's settings as a table (see below). |
| `v` | The values from the source as a table (the plugin's `values`). What is unknown is **missing** (`nil`). |
| `fps` | The frame rate (settable). |

The drawing area is:

- for a **strip**: `W` = number of LEDs of the segment, `H` = 1;
- for the **Master in matrix mode**: the Master's matrix (`W` × `H`);
- on a **Slave**: the Slave's area: a HUB75 panel `W` × `H`, or a strip with its LED count.

### `settings`

The plugin's settings, under their `key`:

| Type | In the script |
|---|---|
| `number`, `segment` | number |
| `switch` | `true` / `false` (also `false`, never `nil`) |
| `color` | **number `0xRRGGBB`**, for example `#00ff80` is `0x00FF80` (`65408`). Splitting: `r = (c // 65536) % 256`, `g = (c // 256) % 256`, `b = c % 256` |
| `text`, `list`, `effect` | text |
| `password` | **not present** |

Three details about the **segment** come with them, entered by HyperLED itself (names with an underscore, so that they do not clash with settings of your own):

| Name | Meaning |
|---|---|
| `settings._leds` | How many LEDs the segment has. |
| `settings._first` | Where the segment's LEDs start in the chain (the k-th LED of the segment is LED `_first + k`). |
| `settings._layout` | `"grid"`: the canvas **is** the segment (a panel, a plain strip, anything on a Slave). `"snake"` or `"rows"`: the Master has a matrix set up, but the segment is a **strip** that hangs on it; its LEDs are the first pixels of the canvas, row by row (`"snake"`: every second row backwards, `"rows"`: every row from the left). To run a bar along such a strip in order, convert the LED number `i` to `x = i % W` (with `"snake"` and an odd row `W - 1 - x`) and `y = i // W`. |

Empty settings are missing (`nil`). **A script never receives passwords:** it may run on a Slave, and everything it receives travels over the air to it - unencrypted.

```lua
local c = settings.colour or 0x00FF00
local r, g, b = (c // 65536) % 256, (c // 256) % 256, c % 256
```

### `v`

The values from `values`, with the same names. Numbers arrive as numbers, text as text, true/false as `true`/`false`. An unknown value is **missing**: `v.temp` is then `nil`. Check that before you calculate with it:

```lua
if v.temp == nil then ... end
local t = v.temp or 0
```

---

## Drawing

| Function | Effect |
|---|---|
| `px(x, y, r, g, b)` | One pixel. `x` from 0 to `W-1`, `y` from 0 to `H-1`, colours 0 to 255 (larger values are limited). Coordinates **outside** are ignored. Decimal numbers are rounded down. |
| `fill(r, g, b)` | All pixels in one colour. |
| `clear()` | Everything black. |
| `hsv(h, s, v)` | Converts a colour and returns `r, g, b`. `h` 0 to 359 (any whole number, it wraps), `s` and `v` 0 to 255. |
| `log(text)` | A message for you as the author (see below). |

The script always draws at **full brightness** (0 to 255). The person's **brightness**, the current limiting and, for HUB75 panels, the driver brightness are applied by HyperLED afterwards. So do not draw "dark" to dim.

The frame stays until the next one is finished: if `frame` draws nothing, the previous frame stays. A frame that aborts with an error is skipped.

---

## Examples

### Progress bar

A bar from the left that fills with a value `progress` (0 to 100), in the colour from the settings:

```lua
function frame(t)
  local share = (v.progress or 0) / 100
  local filled = math.floor(share * W + 0.5)
  local c = settings.colour or 0x00FF00
  local r, g, b = (c // 65536) % 256, (c // 256) % 256, c % 256
  for y = 0, H - 1 do
    for x = 0, W - 1 do
      if x < filled then px(x, y, r, g, b) else px(x, y, 0, 0, 0) end
    end
  end
end
```

### A rainbow that moves

`t` brings the movement:

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

### Warning flash on a state

`update()` only calculates when new values are there; `frame` then flashes:

```lua
local windy = false

function update()  -- new values have arrived
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

The complete example with a scale from the settings is in [`plugins/beispiel-skript-thermometer.json`](../../plugins/beispiel-skript-thermometer.json): the length of the bar follows the temperature, the colour goes from blue through green to red (`hsv(240 - share * 240, …)`).

---

## Language and libraries

Lua 5.4 applies - with **restrictions**, so that a script cannot reach anything but its segment:

| Available | Not available |
|---|---|
| Basic functions (`type`, `tostring`, `tonumber`, `ipairs`, `pairs`, `select`, `setmetatable`, `error`, `assert` …), `math`, `string`, `table` | `os`, `io`, `package`, `debug`, `coroutine`, `utf8`; `require`, `dofile`, `loadfile`, `load`, `collectgarbage`, `warn`, `pcall`, `xpcall`; `string.dump` |

`print` is the same as `log`. **Only text** is loaded, no precompiled code. `error("…")` aborts the call (see errors).

### Numbers: 32 bit

HyperLED uses Lua with **32-bit numbers**: whole numbers up to about ±2 billion, and decimal numbers with **single precision** (about 7 significant digits). That is fast and enough for pixels. Consequences:

- Very large numbers and very precise calculations do not work.
- `t` is a whole number in milliseconds and **wraps after about 24 days**, starting again at 0. Do not calculate with very long periods.
- `/` always gives a decimal number, `//` a rounded-down whole number. For pixel numbers use `math.floor` or `//`.
- Numbers from the source arrive as decimal numbers with single precision (`60` may arrive as `60.00000238`). Round, or compare with some margin (`math.abs(a - b) < 0.01`).

There is no clock and no date; time comes only from `t`. Random numbers: `math.random`.

---

## Limits

| What | Limit |
|---|---|
| Script text | 8 KB |
| Time per call (`frame`, `update`, `init`) | **40 ms**; for areas over 1024 pixels (for example a 64×64 panel) **120 ms**, on the Master as well as on a Slave |
| Memory of the script | 48 KB on the Master, **32 KB** on a Slave (everything the script allocates together) |
| Frame rate (`fps`) | 1 to 60 |
| Nesting depth (parentheses, blocks, functions, metamethods) | 32 levels; deeper gives "C stack overflow" |
| Patterns in `string.find` & co. | at most 100 levels of recursion |

A call that takes longer than its time budget is **aborted** (error "Zeitbudget ueberschritten", time budget exceeded). This also catches endless loops (`while true do end`, endless recursion, patterns that search forever). Exceeding the memory gives "not enough memory".

### How fast is it?

Measured on the ESP32-S3 with a 64×64 area (4096 pixels):

| Script | Time per frame |
|---|---|
| `fill(...)` | about 1 ms |
| All pixels in one colour with `px` | 18-19 ms |
| All pixels with a gradient (`px(x, y, x*4, y*4, b)`) | 20 ms |
| All pixels with `math.sin` (plasma) | 64-85 ms on the Master, 68-75 ms on a Slave (see the note on `sin` below) |

So: simple to medium scripts manage **30 frames per second** (`fps = 30`) or more on a 64×64 panel, compute-heavy ones only about 12 to 15. A plasma over the whole panel takes 70-85 ms per frame and fits into the budget of 120 ms - but there is not much room: a single frame that takes longer once (because the radio happens to need computing time) is aborted; only **three aborts in a row** end the script. On strips Lua costs practically nothing.

**Feed `sin` and `cos` small arguments.** On the ESP32-S3 `math.sin` gets noticeably slower when the argument is larger than about 128 (measured: a 64×64 plasma takes about 82 ms with arguments around 0 to 100, and more than 120 ms from about 200). If you put `t` unchecked into `sin(x / 8 + t * 0.001)`, after two and a half minutes you have a script that fails on the time budget. Reduce the time first, so that the value makes one full turn (`2π ≈ 6.283`) and starts at 0 again - whole multiples of it make no jump:

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

The whole multiples of the turn (`a`, `2 * a`, `-a`) keep the animation continuous. This plasma ran five minutes in a row on a 64×64 panel at 72.8 ms per frame.

Tips for fast scripts:

- Draw **only what changes**: a `fill` followed by a few `px` is far cheaper than setting every pixel.
- Calculate once in `update()` or at the top level (tables, colours), not on every frame.
- Put functions into local variables (`local sin = math.sin`) - faster than access through `math.`.
- Choose a small `fps`.
- Do not create new tables or texts in the frame (memory and clean-up work).

---

## Errors and troubleshooting

- A **syntax error** is reported by the installation with the line: `Skript: script:7: 'end' expected near '<eof>'`.
- A **runtime error** (`error(...)`, calculating with `nil`, wrong type) aborts the call; the message appears in the plugin under **Script** (with `script:LINE:`) and in **Live values**. The previous frame stays.
- **Three failed calls in a row** (or ten in a minute) end the script. It is **not** started again until the person switches the plugin off and on or changes its settings. Until then the plugin's **rules** apply as a fallback.
- **`log(text)`** writes a message for you: at most **one per second** and **80 characters**. It is meant for reading, not as output: `log(v.temp)` shows you what arrives. **Known limitation:** the "last message" in **Live values** currently shows a message of the script only together with an error; while the script runs without errors, a `log()` message is not shown there.
- The line **"Script running · 20.5 ms per frame"** in the plugin list shows the average time of a frame. If it is close to the time budget, things get tight.

---

## Where does the script run?

- If the segment belongs to the **Master**, the script runs on the Master (in a task of its own that never holds up the rest).
- If it belongs to a **Slave**, the Master sends script and values over, and the Slave runs it **itself**. Only values go over the radio link, never pixels. This requires **Slave firmware 0.3.000 or later**; an older Slave is recognised, and the plugin shows what its rules provide.

**One** script runs per segment. After a restart of the Master the plugin sends the script again by itself.

For `needs.script`: script level **1** is this interface. Later extensions (for example time of day or text) raise the level, not the file format. A plugin with a higher level is shut down on older devices and runs by itself after an update.
