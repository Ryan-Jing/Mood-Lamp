# Mood-Lamp

Synchronized mood lamp between two paired devices. Each lamp is a Seeed XIAO ESP32-C3 with a
single WS2812B RGB LED and a dual-stage button. A lamp shows its **peer's** mood by default; you
set your own mood with the button, it's pushed to a small server, and the peer lamp polls and
renders it.

- **Firmware** (`Firmware/`) — PlatformIO + Arduino, two FreeRTOS tasks (UI + networking). See [`Firmware/README.md`](Firmware/README.md).
- **Server** (`Server/`) — FastAPI + SQLite on a Raspberry Pi, exposed publicly via Tailscale Funnel.
- **Utils** (`Utils/`) — `moods.yaml` (mood source of truth), the mood-header generator, and the BLE Wi-Fi provisioning script. See [`Utils/README.md`](Utils/README.md).
- **Docs** (`Docs/`) — architecture and CI/CD notes.
- **Electrical** / **Mechanical** — KiCad project and the printable enclosure STLs.

---

## Setup guide

### 1. Flash the firmware (first time only)

Skip to step 2 if the lamp is already flashed. Requires [PlatformIO Core](https://platformio.org/install/cli)
(`pio`) on your computer.

```bash
cd Firmware
```

```bash
cp include/secrets.example.h include/secrets.h
```

Open `include/secrets.h` and fill in `SERVER_URL`, `DEVICE_TOKEN` (one per lamp, from
`Server/database_init.py`), and `ROOT_CA`. Then connect the lamp by USB-C and flash it:

```bash
pio run -e seeed_xiao_esp32c3 -t upload
```

Do **not** press the lamp's button while flashing.

### 2. Power the lamp over USB-C

Plug the XIAO's USB-C port into any 5 V USB source (charger, laptop, power bank). The lamp boots
in about two seconds and the LED tells you where it is:

| LED | Meaning | What to do |
|---|---|---|
| Cyan blink | BLE provisioning — no Wi-Fi credentials saved | Go to step 3 |
| Warm-white breath | Connected, peer hasn't set a mood yet | Nothing, it's ready |
| Animated mood colour | Connected, showing the peer's mood | Nothing, it's ready |
| Orange blink | Wi-Fi problem | Re-provision (step 3) or check the network |

If the lamp already has credentials but you want to re-provision, hold the button fully pressed
for 5 seconds to force BLE provisioning on.

### 3. Send Wi-Fi credentials with `Utils/provision.py`

Do this on a computer with Bluetooth, within a few metres of the lamp, while the LED is blinking
cyan. Provisioning stays open for 2 minutes per session.

```bash
cd /path/to/Mood-Lamp
```

```bash
python3 -m venv .venv
```

```bash
source .venv/bin/activate
```

```bash
pip install bleak
```

```bash
python3 Utils/provision.py
```

The script prompts for the network name and password, finds the lamp advertising as `MoodLamp`,
and writes the credentials over BLE:

```
Wi-Fi SSID: MyNetwork
Wi-Fi password: ********
Scanning for 'MoodLamp' (the lamp LED should be blinking cyan)...
Connected. Sending credentials...
Credentials sent. Waiting for the lamp to connect to Wi-Fi...
Lamp status: connecting
```

The lamp saves the credentials to flash (they survive reboots and power loss), leaves BLE, and
joins the network. The LED stops blinking cyan within a few seconds. Notes:

- 2.4 GHz networks only — the ESP32-C3 has no 5 GHz radio.
- If the lamp isn't found, it's not in provisioning mode; hold the button fully pressed for
  5 seconds and run the script again.
- `python3 Utils/ble_check.py` lists nearby BLE devices if you want to confirm the lamp is
  advertising at all.

### 4. Using the button

The button is a dual-action pushbutton: pressing part-way is a **half press** (first detent), and
pressing all the way down is a **full press**. Every gesture fires when you *release*.

| Gesture | What it does |
|---|---|
| Half tap | Only in mood-select mode: step to the next mood |
| Full tap | Enter mood-select mode → then confirm the mood and send it to your peer |
| Full press, hold 5 s | Turn BLE provisioning on (or off if it's already on) |
| Full press, hold 10 s | Erase the saved Wi-Fi credentials and reopen BLE provisioning |

To set your mood:

1. **Full tap** — the LED switches from your peer's mood to your own. (This only works while the
   lamp is connected; if it isn't, nothing happens.)
2. **Half tap** repeatedly until the LED shows the mood you want. It wraps around from Working
   back to Excited.
3. **Full tap** — the mood is posted to the server and the LED goes back to showing your peer's
   mood. Their lamp picks it up within 10 seconds.

Half taps do nothing outside mood-select mode, so brushing the button won't change anything. Hold
times are measured from the moment the *full* stage engages, so resting at the half press first
doesn't count toward the 5 s and 10 s timers.

---

## Moods

Colours and patterns are defined in [`Utils/moods.yaml`](Utils/moods.yaml) and generated into
`Firmware/include/moods.h`. The ID below is the `mood_id` the firmware and server exchange.

Three **status** moods are shown by the lamp but are never user-selectable (mood-select skips
them):

| ID | Mood | Colour | Pattern | Period |
|---|---|---|---|---|
| 0 | Idle / default | soft warm white | breath | 6000 ms |
| 1 | BLE provisioning | cyan | blink | 1000 ms |
| 2 | No Wi-Fi | reddish orange | blink | 500 ms |

The user-selectable moods:

| ID | Mood | Colour(s) | Pattern | Period |
|---|---|---|---|---|
| 3 | Excited | bright yellow | breath | 1000 ms |
| 4 | Happy | rainbow (7 colours) | fade | 1000 ms |
| 5 | Sad | dark blue | breath | 3000 ms |
| 6 | Upset | dark red | breath | 1000 ms |
| 7 | Anxious | dark purple → purple → violet → red | breath alternate | 800 ms |
| 8 | Deep Breaths | dark green | breath | 8000 ms |
| 9 | Love | dark pink → pink → bright pink → light purple | fade | 1000 ms |
| 10 | Heepy | bright pink | blink | 800 ms |
| 11 | Hungry | dark brownish yellow | breath | 1000 ms |
| 12 | Tired | dark red ↔ dark orange | fade | 10000 ms |
| 13 | Working | warm orange | solid | — |

Patterns: `solid`, `blink` (on for the first half of the period), `breath` (dim → bright → dim),
`alternate` (hard switch between colours), `fade` (smooth cross-fade), and `breath_alternate`
(one breath per colour, advancing to the next each cycle).

To change moods, edit [`Utils/moods.yaml`](Utils/moods.yaml), then regenerate the firmware header
and re-flash:

```bash
cd /path/to/Mood-Lamp
```

```bash
python3 Utils/generate_moods.py
```

Both lamps must run the same mood table — IDs are what travel over the wire, so a lamp with an
older table will render the wrong mood.
