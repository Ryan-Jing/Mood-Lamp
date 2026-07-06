# Mood Lamp — Architecture

A mini device that shares one user's "mood" to a second **paired** device wirelessly, shown as
RGB LED colours/patterns. Exactly two devices talk to each other through a small server; each
device displays the *other* device's mood by default, and the user sets their own mood with a
single physical button.

---

## Key decisions

| Area | Choice | Notes |
|---|---|---|
| MCU | Seeed XIAO **ESP32-C3** (single-core RISC-V, WiFi + BLE) | Powered via on-board USB-C. |
| LED | SparkFun **13282** — a single **WS2801** RGB LED | SPI-like: data (SDI) + clock (CKI), 24 bits/colour, latch by holding clock low ≥500 µs. |
| Firmware framework | **PlatformIO + Arduino-ESP32** | FreeRTOS is built into the core. |
| Device ↔ server | **HTTPS + polling** to start; MQTT-over-TLS as a later upgrade | Keep the transport behind an interface so the swap is cheap. |
| Server | **Python + FastAPI** behind **Caddy** (auto-TLS) | Same language as the BLE provisioning app. |

**Guiding security principle:** the device's identity comes from its *credential*, never from an ID
in the request body. The server resolves "who is calling" from the auth token, then looks up that
device's mood/peer.

---

## 1. Repository layout

```
mood-lamp/
├── Docs/
│   └── architecture.md        # this file
├── shared/
│   └── moods.yaml             # single source of truth: mood id -> name, colour, pattern
├── tools/
│   └── gen_moods.py           # moods.yaml -> Firmware/include/moods.h (run on your PC)
├── Firmware/                  # ESP32-C3 (PlatformIO)
│   ├── platformio.ini
│   ├── include/               # headers (moods.h is GENERATED)
│   └── src/                   # modules (see §2)
├── provisioning/             # Python BLE setup app
└── server/                   # FastAPI backend
```

The important idea is **`shared/moods.yaml` as one source of truth**: firmware, server, and the
provisioning app must all agree on what "mood 7" means. Edit the YAML, regenerate, and everything
stays in sync.

---

## 2. Firmware architecture (ESP32-C3)

### 2.1 Module layout

```
include/                      src/
├── app_state.h               ├── main.cpp            # tasks + state-machine dispatch
├── moods.h   (GENERATED)     ├── app_state.cpp       # per-state handlers
├── identity.h                ├── identity.cpp
├── hal/                      ├── hal/
│   ├── led.h                 │   ├── led.cpp         # WS2801 driver + led_render()
│   ├── button.h              │   ├── button.cpp      # debounced button + FSM handlers
│   └── led_effects.h         │   └── led_effects.cpp # pattern math (solid/breathe/blink)
├── net/                      ├── net/
│   ├── wifi_mgr.h            │   ├── wifi_mgr.cpp
│   ├── api_client.h          │   ├── api_client.cpp
│   └── ble_provision.h       │   └── ble_provision.cpp
└── storage/                  └── storage/
    └── config_store.h            └── config_store.cpp
```

Layering rule: headers include **down** the stack (app → hal), never in a cycle. When two headers
seem to need each other, one side uses a forward declaration instead of a full include.

### 2.2 Two FreeRTOS tasks

- **`vLampTask` (app/UI)** — fast fixed tick (~100 Hz). Reads the button, runs the `AppState`
  machine, renders the LED. **Never blocks.**
- **`vCommsTask` (wireless)** — owns WiFi + BLE + server I/O. **May block** (WiFi/HTTP calls). Uses
  a queue-with-timeout so one call both waits for a "push my mood" command and polls the server on
  an interval.

Each task owns its own state. They exchange only small data: a mutex-protected snapshot
(peer mood, link status) that the lamp task reads, and a queue for "push this mood" going the other
way. `LampState` stays task-local, so passing it by reference to the FSM handlers needs no locking.

### 2.3 The `AppState` machine

| State | Meaning |
|---|---|
| `PROVISIONING` | No WiFi credentials — BLE advertising so the Python app can send SSID/password. |
| `CONNECTING` | Joining WiFi + authenticating to the server. |
| `SHOW_MOOD` | Default — display the peer's current mood (or a default colour if no peer). |
| `SELECT_MOOD` | User is choosing their own mood: tap to cycle, hold to confirm. |

Button gestures are **decided on release, gated by hold duration** (tap vs 5 s hold vs 10 s hold),
which keeps each state's gesture unambiguous and avoids one hold "carrying over" into the next state.

### 2.4 LED pipeline (mood → pixels)

```
shared/moods.yaml  --gen_moods.py-->  include/moods.h (enum + MOOD_TABLE + mood_def())
                                             │
led_render(mood)  --mood_def(mood)-->  { r,g,b, pattern, period_ms }
                                             │
                          led_effects: mood_frame(def, millis()) --> instantaneous r,g,b
                                             │
                                     led_write(r,g,b)  --24 bits + 500µs latch-->  WS2801
```

- **`moods.h`** (generated) holds the `Moods` enum, the `MoodDef` table, and `mood_def()`.
- **`led_effects`** turns *(mood definition, time)* into an instantaneous colour (solid / breathe /
  blink) — a pure function, easy to test.
- **`led`** is the WS2801 hardware driver: `led_init()`, `led_write(r,g,b)`, and `led_render(mood)`
  which glues the two together each tick.

### 2.5 BLE + WiFi + persistence

- Run **BLE only during `PROVISIONING`**, then de-init to free RAM (use NimBLE-Arduino).
- GATT service exposes write characteristics for `wifi_ssid`, `wifi_pass`, `server_url`, plus a
  read-only `device_id` and a `status` notify. The **device secret is never writable over BLE**.
- Store WiFi creds, `device_id`, `device_secret`, `server_url`, and `last_mood` in **NVS**.

---

## 3. Server architecture

Minimal FastAPI service, but with real security.

### 3.1 Data model

- **Device**: `device_id` (PK, baked at flash), `secret_hash`, `pair_id`, `last_seen`.
- **Pair**: `pair_id`, `device_a`, `device_b` — enforces "exactly two devices talk."
- **MoodState**: `device_id`, `mood_id`, `updated_at`, `version` (monotonic, for change detection).

### 3.2 API (all over HTTPS, all authenticated)

- `POST /v1/mood` — body `{ "mood_id": 7 }`. Caller identity from the **auth token**, not the body.
  Validates `mood_id` in range, updates that device's mood, bumps `version`.
- `GET /v1/peer/mood` — returns the paired partner's mood + `version`. Supports `If-None-Match` on
  `version` → cheap `304 Not Modified` polling (this is the "only update on change" mechanism).
- `GET /v1/health` — unauthenticated liveness.

### 3.3 Security checklist

- **TLS everywhere** (Caddy auto-provisions Let's Encrypt; the device verifies the CA / pins the cert).
- **Per-device secrets**, stored hashed. No shared global secret.
- **Server derives identity from the credential**, never from a client-claimed ID.
- **Authorization:** a device may only write its own mood and read its paired partner's.
- **Rate limiting** per device; **input validation** via Pydantic (mood in known range).
- **BLE provisioning hardening:** passkey bonding or a PSK-encrypted WiFi password; secret not
  settable over BLE.
- Minimal data, no PII, never log secrets.

### 3.4 Pairing

Either **flash-time pairing** (bake the same `pair_id` into both units) or a **pairing-code flow**
(user links the two via a short-lived code). Start with flash-time; add codes later.

---

## 4. Suggested build order (de-risk hardware first)

1. **LED + button HAL** — WS2801 colours + reliable debounced long/short press.
2. **State machine + mood-select UX** — fully offline.
3. **Server skeleton** — FastAPI, SQLite, Bearer auth, plain HTTP locally.
4. **Firmware ↔ server over WiFi** — hardcoded creds first, then TLS + cert verification.
5. **BLE provisioning** — replace hardcoded creds with the BLE flow + Python app.
6. **Pairing + peer display** — two physical devices showing each other's mood end to end.
7. **Harden** — HMAC signing, rate limiting, Caddy auto-TLS deploy, optional flash encryption.
