# Mood Lamp - Architecture

Two paired ESP32 mood lamps share state through a small server. Each lamp shows the peer's mood by
default, while the local button selects this device's own mood and posts it to the server.

## Key Decisions

| Area | Current choice | Notes |
|---|---|---|
| MCU | Seeed XIAO ESP32-C3 | Single-core RISC-V board with Wi-Fi and BLE. |
| Firmware | PlatformIO + Arduino-ESP32 | Uses FreeRTOS tasks from the Arduino core. |
| LED | Single WS2812B/NeoPixel-style RGB LED | Driven by `Adafruit_NeoPixel` on `LED_DIN_PIN`. |
| Provisioning | NimBLE GATT + Python scripts in `Utils/` | BLE is used to send Wi-Fi SSID/password only. |
| Device-to-server | HTTPS polling | The ESP32 posts its mood and polls the peer mood about every 10 seconds. |
| Server | FastAPI + SQLite on the RPi CM5 | `uvicorn` runs under `systemd`; Tailscale Funnel provides public HTTPS. |
| Auth | Per-device bearer token | The server hashes stored tokens and derives identity from the bearer token. |

The server must never trust a device ID from the request body. A device proves who it is with its
bearer token, then the server decides which pair and peer it is allowed to access.

## Repository Layout

```text
Mood-Lamp/
|-- Docs/
|   |-- architecture.md
|   `-- ci-cd.md
|-- Firmware/
|   |-- include/
|   |   |-- config.h
|   |   |-- moods.h                 # generated from Utils/moods.yaml
|   |   |-- secrets.example.h       # safe placeholder for CI/public repo
|   |   |-- secrets.h               # ignored local secrets file
|   |   |-- state.h
|   |   |-- hal/
|   |   `-- net/
|   |-- src/
|   |   |-- main.cpp                # task creation and top-level state loops
|   |   |-- state.cpp               # mutex/queue shared task interface
|   |   |-- hal/
|   |   `-- net/
|   |-- test/
|   `-- platformio.ini
|-- Server/
|   |-- app.py                     # FastAPI API
|   `-- database_init.py           # SQLite schema + token generation
|-- Utils/
|   |-- moods.yaml                 # source of truth for mood IDs
|   |-- generate_moods.py          # YAML -> Firmware/include/moods.h
|   |-- ble_check.py
|   `-- provision.py
`-- .github/workflows/ci.yml
```

`Utils/moods.yaml` is the mood source of truth. After changing it, regenerate
`Firmware/include/moods.h` with `python Utils/generate_moods.py`.

## Firmware Architecture

### Module Layout

```text
Firmware/include/                 Firmware/src/
|-- config.h                      |-- main.cpp
|-- moods.h                       |-- state.cpp
|-- state.h                       |-- hal/
|-- secrets.example.h             |   |-- button.cpp
|-- secrets.h                     |   |-- led.cpp
|-- hal/                          |   `-- led_effects.cpp
|   |-- button.h                  `-- net/
|   |-- led.h                         |-- api.cpp
|   `-- led_effects.h                 |-- ble.cpp
`-- net/                              `-- wifi.cpp
    |-- api.h
    |-- ble.h
    `-- wifi.h
```

Important boundaries:

- `main.cpp` owns the two FreeRTOS tasks and the top-level state transitions.
- `state.cpp/h` owns cross-task communication: mutex-protected comms state and peer mood, plus queues
  for posted moods and user commands.
- `button.cpp` reads/debounces the physical button and converts gestures into state changes or
  `UserCommand` messages.
- `led_effects.cpp` is pure animation math and is compiled into native unit tests.
- `led.cpp` is hardware-facing and owns `Adafruit_NeoPixel`.
- `wifi.cpp` owns Wi-Fi connect/disconnect and NVS Wi-Fi credential storage.
- `ble.cpp` owns the BLE provisioning GATT service.
- `api.cpp` owns HTTPS calls to the FastAPI server.

### Task Model

The firmware runs two long-lived tasks:

- `vLampTask` is the UI task. It reads the button every 10 ms, chooses the lamp state, and renders the
  LED. It should stay non-blocking.
- `vCommsTask` is the network task. It owns BLE, Wi-Fi, SNTP readiness, HTTP requests, API retry
  counting, and NVS Wi-Fi credential operations. This task may block on network operations.

The tasks share only small pieces of data:

- `CommsStatus` through `shared_set_net_state()` / `shared_get_net_state()`.
- Peer mood through `shared_set_peer_mood()` / `shared_get_peer_mood()`.
- Local mood posts through `shared_post_mood()` / `shared_get_posted_mood()`.
- Long-hold user commands through `shared_post_user_command()` / `shared_take_user_command()`.

### State Model

`LampState` is local to `vLampTask`.

| Field | Meaning |
|---|---|
| `self_mood` | This lamp's locally selected mood. This is posted to the server and shown on the peer lamp. |
| `peer_mood` | The paired lamp's latest mood. This is what this lamp displays in normal mode. |
| `application_state` | UI/rendering state: `BLE_STATUS`, `NET_STATUS`, `SHOW_MOOD`, or `SELECT_MOOD`. |
| `button_state` / timers | Debounced input and hold-duration bookkeeping. |

`NetState` is local to `vCommsTask`.

| Field | Meaning |
|---|---|
| `comms_status` | Network state: `BLE_PROVISIONING`, `BLE_CONNECTED`, `NET_CONNECTING`, `NET_CONNECTED`, or `NET_DISCONNECTED`. |
| `wifi_credentials` | Last loaded or newly provisioned Wi-Fi SSID/password. |
| `peer_mood` / `peer_version` | Last peer mood returned by the server and its ETag/version. |
| `mood_to_post` / `has_mood_to_post` | Pending local mood that must be sent to the server. |
| `wifi_retry_count` / `poll_retry_count` | Retry counters used before backing out of network work. |

### Button Gestures

Button behavior is intentionally decided on release so one hold does not trigger multiple actions.

| Gesture | State requirement | Result |
|---|---|---|
| Short press | `SELECT_MOOD` | Cycle `self_mood`. |
| Hold `MOOD_SET_TIMER` to `BLE_SET_TIMER` | `SHOW_MOOD` and `NET_CONNECTED` | Enter `SELECT_MOOD`. |
| Hold `MOOD_SET_TIMER` to `BLE_SET_TIMER` | `SELECT_MOOD` and `NET_CONNECTED` | Post `self_mood` and return to `SHOW_MOOD`. |
| Hold at least `BLE_SET_TIMER` | Any state | Toggle BLE provisioning on/off with `USER_COMMAND_START_BLE` or `USER_COMMAND_STOP_BLE`. |
| Hold at least `WIFI_CLEAR_TIMER` | Any state | Clear NVS Wi-Fi credentials and start BLE provisioning. |

Mood selection is blocked when the lamp is not connected to the server. If Wi-Fi/API connectivity is
lost while selecting a mood, the lamp returns to `SHOW_MOOD`.

### LED Pipeline

```text
Utils/moods.yaml
    |
    `-- Utils/generate_moods.py
            |
            `-- Firmware/include/moods.h
                    |
                    `-- get_mood_definition(mood)
                            |
                            `-- mood_frame(def, millis(), r, g, b)
                                    |
                                    `-- led_write(r, g, b)
```

`led_effects.cpp` contains the testable animation function `mood_frame()`. `led.cpp` wraps that pure
logic with the physical NeoPixel driver.

### BLE, Wi-Fi, and NVS

On boot, the comms task checks NVS for Wi-Fi credentials:

- If no credentials exist, the device starts BLE provisioning.
- If credentials exist, BLE stays off and the device tries to connect to Wi-Fi.

The BLE service advertises as `MoodLamp` and exposes characteristics for:

- SSID write
- password write
- apply write
- status read/notify

When the Python provisioning script writes SSID/password and then writes the apply characteristic,
`vCommsTask` saves the credentials to NVS, notifies `"connecting"`, stops BLE, and transitions to
`NET_CONNECTING`.

Only Wi-Fi credentials are stored in NVS today. Server configuration lives in the ignored local
`Firmware/include/secrets.h` file:

```cpp
#define SERVER_URL "https://your-tailnet-or-funnel-host"
#define DEVICE_TOKEN "one-device-token-from-database_init.py"
#define ROOT_CA R"EOF(
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
)EOF"
```

The public `secrets.example.h` exists only so CI and new clones can compile without real secrets.

### Network Behavior

After Wi-Fi joins, `api_wait_for_time()` waits for SNTP before HTTPS requests. This matters because
certificate validation requires a sane system time.

When connected:

- pending local moods are sent with `POST /v1/mood`;
- the peer mood is polled with `GET /v1/peer/mood`;
- the last known server `version` is sent as `If-None-Match`;
- a `304 Not Modified` response is treated as a successful no-op.

If Wi-Fi drops or API calls repeatedly fail, the comms task disconnects Wi-Fi and returns to
`NET_CONNECTING`. After the configured Wi-Fi retry limit, it enters `NET_DISCONNECTED`; the lamp shows
the idle mood until the user chooses to re-enter BLE provisioning or clear Wi-Fi credentials.

## Server Architecture

The current server is a small FastAPI application backed by SQLite:

- `Server/app.py` defines the API.
- `Server/database_init.py` creates the database tables and prints raw device tokens once.
- `MOOD_DB` selects the database path.
- `MOOD_COUNT` controls the valid mood ID range.

The intended production shape on the RPi CM5 is:

```text
ESP32
  |
  | HTTPS
  v
Tailscale Funnel hostname
  |
  | proxy to localhost
  v
uvicorn on 127.0.0.1:8000
  |
  v
FastAPI app + SQLite database
```

Tailscale Funnel terminates public HTTPS and proxies to local HTTP on the Pi. The FastAPI process can
therefore bind to `127.0.0.1:8000` under `systemd`; it does not need to listen directly on the public
network.

### Data Model

`database_init.py` creates three tables:

| Table | Purpose |
|---|---|
| `pairs` | Pair records, keyed by `pair_id`. |
| `devices` | Device records with `device_id`, `pair_id`, `display_name`, `token_hash`, and `last_seen`. |
| `mood_state` | Current mood for each device: `device_id`, `mood_id`, `version`, and `updated_at`. |

The raw device token is printed only when the device row is first created. The database stores only
`sha256(token)`, so a lost token should be replaced by creating/updating that device's token.

### API

| Endpoint | Auth | Purpose |
|---|---|---|
| `GET /v1/health` | No | Liveness check. |
| `GET /v1/me` | Bearer token | Debug/identity check for the current token. |
| `POST /v1/mood` | Bearer token | Set the caller's own mood. Body: `{ "mood_id": 4 }`. |
| `GET /v1/peer/mood` | Bearer token | Return the paired device's mood, version, and update time. |

`POST /v1/mood` validates `mood_id` using Pydantic: `0 <= mood_id < MOOD_COUNT`.

`GET /v1/peer/mood` requires exactly one peer in the caller's pair. It returns an `ETag` based on the
peer mood version. If the ESP32 sends the matching `If-None-Match` value, the server returns `304`.

### Security Notes

Safe to commit:

- source code;
- `Firmware/include/secrets.example.h`;
- docs;
- generated `Firmware/include/moods.h` if you want the firmware build to work without rerunning the
  generator.

Do not commit:

- `Firmware/include/secrets.h`;
- `.env`;
- SQLite databases such as `mood.db`;
- raw device tokens;
- private keys, PEM files, or certificate material you intend to keep private;
- `.venv`, `__pycache__`, `.pyc`, PlatformIO build output, and firmware binaries.

BLE provisioning is currently a convenience channel for Wi-Fi setup, not a hardened ownership flow.
Anyone nearby who can connect while provisioning is active may be able to write Wi-Fi credentials. Keep
BLE off during normal operation, and consider passkey bonding or an app-side pairing secret before
using this outside a trusted environment.

## Build Order

The current project has these pieces in place:

1. LED/button HAL and mood animation logic.
2. Two-task firmware state model.
3. BLE Wi-Fi provisioning into NVS.
4. Wi-Fi connect/retry/disconnect behavior.
5. FastAPI + SQLite mood server.
6. HTTPS polling client on the ESP32.
7. CI firmware build, native tests, and cppcheck.

Useful next work:

1. Stop printing Wi-Fi passwords in debug logs before sharing logs or demos.
2. Add native tests around the button gesture/state-machine behavior.
3. Add server-side tests for auth, mood validation, pair isolation, and ETag/304 behavior.
4. Add a real deployment note for renewing/replacing the ESP32 trusted root CA.
