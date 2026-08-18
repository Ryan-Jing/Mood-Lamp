# Mood Lamp — Firmware

Firmware for a two-device "mood lamp" pair. Each lamp is a Seeed XIAO ESP32-C3 with one
WS2812B RGB LED and one dual-stage pushbutton. A lamp's default display is the **peer's**
mood: the user sets their own mood locally with the button, the firmware pushes it to a
shared server over HTTPS, and the peer lamp polls the server and renders it as a
colour/pattern. Wi-Fi credentials are provisioned over BLE from a desktop app.

---

## Hardware

| Component | Part | Connection |
|---|---|---|
| MCU | Seeed XIAO ESP32-C3 (single-core RISC-V, Wi-Fi + BLE) | USB-C for power/flash/serial |
| RGB LED | SparkFun WS2812B Breakout (part 13282) | DIN → GPIO4 (XIAO `D2`), DO unconnected |
| Button | E-Switch **PB300DTQ** dual-action pushbutton | pin 1 → GND, pin 2 → `D7`, pin 3 → `D8` |

### Wiring notes

- **WS2812B**: single-wire self-clocked protocol, driven by the Adafruit NeoPixel library
  (`NEO_GRB + NEO_KHZ800`). Power the LED from **3.3 V**, not 5 V: with a 5 V supply the
  ~3.5 V logic-high threshold is above the ESP32-C3's 3.3 V output. At 3.3 V supply the LED
  is in spec, slightly dimmer.
- **Button**: both stage pins use internal pull-ups (`INPUT_PULLUP`), active low. No
  external resistors needed.
- **`D8` is GPIO8, an ESP32-C3 strapping pin.** Normal boot is unaffected, but if the
  button is held fully pressed while entering download mode (BOOT held during reset),
  GPIO8 reads LOW and flashing mode will not engage. Don't press the button while flashing.
- **`D7` is GPIO20 (UART0 RX).** Fine here because `Serial` uses USB-CDC, but the hardware
  UART can't be used for input while the button owns this pin.

---

## Project setup

Built with [PlatformIO](https://platformio.org/) on the Arduino framework.

```sh
cd Firmware

# 1. Create your secrets file (never committed)
cp include/secrets.example.h include/secrets.h
#    ...then fill in SERVER_URL, DEVICE_TOKEN, and ROOT_CA (PEM string)

# 2. Build + flash + monitor (XIAO ESP32-C3 over USB)
pio run -e seeed_xiao_esp32c3 -t upload
pio device monitor            # 115200 baud

# 3. Run host-side unit tests (no hardware needed)
pio test -e native
```

Library dependencies (resolved automatically by PlatformIO): Adafruit NeoPixel,
NimBLE-Arduino, ArduinoJson.

`include/moods.h` is **auto-generated** — edit `Utils/moods.yaml` at the repo root and
re-run the generator (`Utils/generate_moods.py`) instead of editing it by hand.

Debug logging is controlled by `PRINT_DEBUG` in `include/config.h`; comment it out for a
quiet build. Note that with it enabled, received Wi-Fi credentials are echoed to serial.

---

## File structure

```
Firmware/
├── platformio.ini            # Envs: seeed_xiao_esp32c3 (target), native (unit tests)
├── include/
│   ├── config.h              # Timeouts, poll interval, retry limits, PRINT_DEBUG
│   ├── moods.h               # AUTO-GENERATED mood enum + colour/pattern table
│   ├── secrets.h             # Server URL, device token, root CA (from secrets.example.h)
│   ├── state.h               # LampState / NetState types + shared task interface
│   ├── hal/
│   │   ├── button.h          # ButtonState/ButtonEvent enums, gesture handler API
│   │   ├── led.h             # LED driver API
│   │   └── led_effects.h     # Pattern animation (pure logic, host-testable)
│   └── net/
│       ├── api.h             # HTTP mood-server client API
│       ├── ble.h             # BLE provisioning service API
│       └── wifi.h            # Wi-Fi connect + NVS credential storage API
├── src/
│   ├── main.cpp              # Entry point; vLampTask + vCommsTask state machines
│   ├── state.cpp             # Mutex/queue-backed shared state between the two tasks
│   ├── hal/
│   │   ├── button.cpp        # Debounce, press-cycle event decoding, gesture handlers
│   │   ├── led.cpp           # WS2812B driver (Adafruit NeoPixel, GPIO2)
│   │   └── led_effects.cpp   # Solid/blink/breath/alternate/fade frame computation
│   └── net/
│       ├── api.cpp           # HTTPS POST/GET of moods (Bearer auth, ETag polling)
│       ├── ble.cpp           # NimBLE provisioning service (SSID/pass/apply/status)
│       └── wifi.cpp          # STA connect + Preferences (NVS) credential store
├── test/
│   └── test_logic/           # Unity tests for led_effects (run via `pio test -e native`)
└── docs/                     # Doxygen styling (see Doxyfile)
```

### Architecture

Two FreeRTOS tasks, created in `setup()`, communicate only through the mutex/queue-guarded
interface in `state.cpp`:

- **`vLampTask`** — reads the button, runs the `AppState` machine, renders the LED every
  10 ms. Never blocks on the network.
- **`vCommsTask`** — runs the `CommsStatus` machine: BLE provisioning, Wi-Fi connection,
  SNTP time sync, and server sync. Receives user commands (start/stop BLE, clear Wi-Fi)
  from the lamp task via a queue.

---

## Button (PB300DTQ)

The E-Switch **PB300DTQ** is a momentary double-action pushbutton: the first circuit
closes at mid travel and the second at full travel (2.0 mm total; ~600 g to the first
detent, ~1150 g to the second). Pin 1 is common (GND); pin 2 grounds at **half press**
(→ `D7`) and pin 3 grounds additionally at **full press** (→ `D8`). At full press both
signal pins are shorted to GND.

### Debounce and states

`get_button_state()` debounces each stage pin independently (50 ms) and folds them into
one `ButtonState`:

| `D7` (half stage) | `D8` (full stage) | State |
|---|---|---|
| HIGH | HIGH | `BUTTON_RELEASED` |
| LOW | HIGH | `BUTTON_HALF_PRESSED` |
| any | LOW | `BUTTON_FULL_PRESSED` |

The full stage alone decides `BUTTON_FULL_PRESSED`, since a full press grounds both pins.

### Events

A full press mechanically passes through the half stage on the way down *and* up, so raw
states can't be used as gestures directly. `update_button_event()` tracks each press
cycle (latching whether it ever reached full) and emits **one event on release**:

| Event | Meaning |
|---|---|
| `BUTTON_EVENT_HALF_TAP` | Pressed and released without ever reaching the full stage |
| `BUTTON_EVENT_FULL_TAP` | Reached full, released before `BLE_SET_TIMER` (5 s) |
| `BUTTON_EVENT_FULL_HOLD_BLE` | Full stage held ≥ 5 s and < 10 s |
| `BUTTON_EVENT_FULL_HOLD_WIFI` | Full stage held ≥ `WIFI_CLEAR_TIMER` (10 s) |

Hold time is measured from when the **full** stage engages — resting at half press first
doesn't count toward the timers.

### Gesture map

`handle_user_button_commands()` consumes global gestures first; per-state handlers
(`show_mood_button_handle` / `select_mood_button_handle`) get unconsumed events.

| Gesture | SHOW_MOOD | SELECT_MOOD | BLE provisioning/connected |
|---|---|---|---|
| Half tap | — | Cycle to next mood | — |
| Full tap | Enter SELECT_MOOD (requires `NET_CONNECTED`) | Confirm: post mood, back to SHOW_MOOD | **Exit BLE immediately** (`USER_COMMAND_STOP_BLE`) |
| Full hold 5 s | Start BLE provisioning | Start BLE provisioning | Stop BLE |
| Full hold 10 s | Clear Wi-Fi credentials → re-enter provisioning | same | same |

Half taps are deliberately inert outside SELECT_MOOD: the half stage is easy to hit, so
casual presses in SHOW_MOOD do nothing.

### Lamp application states (`AppState`)

| State | LED shows | Entered by |
|---|---|---|
| `SHOW_MOOD` | Peer's mood (or `IDLE` pattern if disconnected) | Default; comms status `NET_*` |
| `SELECT_MOOD` | Your own candidate mood while scrolling | Full tap in SHOW_MOOD |
| `BLE_STATUS` | `BLE` pattern (cyan blink) | Comms status `BLE_PROVISIONING` / `BLE_CONNECTED` |
| `NET_STATUS` | `NO_WIFI` pattern (orange blink) | (reserved) |

`SELECT_MOOD` is exited by confirming a mood, or automatically if the network drops.

---

## LED (WS2812B)

One WS2812B pixel on GPIO4 (XIAO `D2`), driven by Adafruit NeoPixel. Every lamp-task
iteration calls `led_render(mood)`, which looks up the mood's `MoodDefinition` (up to
`MAX_MOOD_COLOURS` = 8 RGB colours, a pattern, and a period) and computes the current frame
with `mood_frame()` in `led_effects.cpp` — pure logic with no hardware dependency, which is
what the native unit tests cover. `led_render` only re-latches the pixel when the computed
colour changes.

Patterns: `SOLID`, `BLINK` (on for the first half of the period), `BREATH` (raised-cosine,
gamma-corrected brightness ramp), `ALTERNATE` (hard switch between colours each period),
`FADE` (smooth cross-fade between colours), `BREATH_ALTERNATE` (one breath per colour,
advancing to the next each cycle). A `period` of 0 renders the base colour solid.

The mood table is generated from `Utils/moods.yaml` into `moods.h`. The three **status**
moods — `IDLE` (warm-white breath), `BLE` (cyan blink), `NO_WIFI` (orange blink) — are never
user-selectable; scrolling to pick a mood skips them (`FIRST_SELECTABLE_MOOD`). The eleven
user moods:

| # | Mood | Colour(s) | Pattern | Period |
|---|---|---|---|---|
| 1 | Excited | bright yellow | breath | 400 ms |
| 2 | Happy | rainbow (7 colours) | fade | 200 ms |
| 3 | Sad | dark blue | breath | 1000 ms |
| 4 | Upset | dark red ↔ dark orange | fade | 1000 ms |
| 5 | Anxious | dark purple → purple → violet → red | breath alternate | 200 ms |
| 6 | Deep Breaths | dark green | breath | 5000 ms |
| 7 | Love | dark pink → pink → bright pink → light purple | fade | 1000 ms |
| 8 | Heepy | bright pink | blink | 500 ms |
| 9 | Hungry | dark brownish yellow | blink | 1000 ms |
| 10 | Tired | purple ↔ blue-purple | fade | 1000 ms |
| 11 | Working | warm orange | solid | — |

---

## Communication

### Comms state machine (`CommsStatus`, in `vCommsTask`)

```
                 no saved creds
  boot ──────────────────────────────► BLE_PROVISIONING ◄──── full hold 10 s (clear Wi-Fi)
   │                                    │  ▲     │ 2 min timeout / full tap
   │ saved creds                 client │  │drop └──────────────┐
   ▼                            connects▼  │                    ▼
  NET_CONNECTING ◄── creds applied ── BLE_CONNECTED      NET_CONNECTING or
   │       ▲                                             NET_DISCONNECTED (no creds)
   │ ok    │ Wi-Fi lost / poll failures
   ▼       │
  NET_CONNECTED ── 12 failed connect attempts ──► NET_DISCONNECTED
```

- `NET_CONNECTING`: joins Wi-Fi (20 s timeout per attempt), then waits for SNTP time sync
  (needed for TLS certificate validation). Retries every 5 s; after 12 failures gives up
  to `NET_DISCONNECTED`.
- `NET_CONNECTED`: uploads any locally-set mood, polls the peer's mood, and falls back to
  `NET_CONNECTING` after 5 consecutive API failures or a Wi-Fi drop.

### BLE provisioning

NimBLE GATT server, device name `MoodLamp`, active for at most 2 minutes per session
(`BLE_PROVISIONING_TIMEOUT_MS`). Service `9a1e0000-8f4a-4b1c-9e2a-1234567890ab`:

| Characteristic | UUID suffix | Access | Purpose |
|---|---|---|---|
| SSID | `...0001` | write | Wi-Fi SSID (max 32 chars) |
| Password | `...0002` | write | Wi-Fi password (max 63 chars) |
| Apply | `...0003` | write | Any write commits the credentials |
| Status | `...0004` | read/notify | `"provisioning"` → `"connecting"` |

Flow: the desktop app writes SSID and password, then writes Apply. The comms task saves
the credentials to NVS (`Preferences`, namespace `wifi`), notifies `"connecting"` on
Status, tears down BLE, and moves to `NET_CONNECTING`. Credentials persist across reboots;
a 10 s full-press hold wipes them and returns to provisioning.

### Server API

HTTPS with certificate pinning (`ROOT_CA`) and per-device Bearer token auth
(`DEVICE_TOKEN`), both from `secrets.h`. The server pairs exactly two devices; "peer"
below is resolved server-side from the token. 5 s request timeout (`API_TIMEOUT_MS`).

**`POST /v1/mood`** — publish this lamp's mood.

```json
{ "mood_id": 6 }
```

**`GET /v1/peer/mood`** — fetch the peer's mood, polled every 10 s
(`PEER_POLL_INTERVAL_MS`). Responses are versioned: the lamp sends
`If-None-Match: "<version>"` and the server replies `304 Not Modified` when nothing
changed, so steady-state polling transfers no body. On change:

```json
{ "mood_id": 6, "version": 42 }
```

`mood_id` is validated against `MOOD_COUNT` before use; malformed responses count toward
the retry limit like HTTP errors.

### Hosting & the `ts.net` DNS gotcha

The server runs on a Raspberry Pi (CM5) and is exposed to the public internet via **Tailscale
Funnel**, so `SERVER_URL` is a `*.ts.net` hostname reachable from anywhere — the ESP32 never
joins the tailnet, it just makes ordinary HTTPS requests to the public Funnel URL.

One gotcha worth knowing: many home routers' **DNS rebind protection blocks `*.ts.net`**
(Tailscale names can resolve into the CGNAT `100.64.0.0/10` range, which rebind protection
treats as suspicious), so the router refuses to resolve the server hostname even though public
resolvers do. Symptom on the lamp:

```
[E][WiFiGeneric.cpp:1583] hostByName(): DNS Failed for <server>.ts.net
```

The firmware works around this by forcing public DNS (`8.8.8.8` / `1.1.1.1`) via
`force_public_dns()` in `wifi.cpp`. It must run **after** `WL_CONNECTED` — once DHCP has
assigned its own DNS — otherwise DHCP overwrites the override. It re-applies on every
`wifi_connect()`, so a DHCP lease renewal that resets DNS self-heals on the next reconnect.

If a lamp on a **new** network still fails after this, that router is *intercepting* outbound
DNS (forcing port 53 through itself); disable DNS rebind protection or allowlist `ts.net` on
that router. Quick confirm via `PRINT_DEBUG`: `DNS1: 8.8.8.8` plus `example.com` resolving but
the server failing pinpoints the `ts.net`-specific block.

> Verifying the server side (from any machine): `dig @8.8.8.8 <server>.ts.net` should return
> Tailscale Funnel IPs, and `curl https://<server>.ts.net/v1/health` should return
> `{"ok":true}`. On the Pi, `tailscale funnel status` should show `/ proxy` to the uvicorn port.
