# Mood-Lamp

Synchronized mood lamp between two paired devices. Each lamp is a Seeed XIAO ESP32-C3 with a
single WS2812B RGB LED and a button. A lamp shows its **peer's** mood by default; you set your
own mood with the button, it's pushed to a small server, and the peer lamp polls and renders it.

- **Firmware** (`Firmware/`) — PlatformIO + Arduino, two FreeRTOS tasks (UI + networking). See [`Firmware/README.md`](Firmware/README.md).
- **Server** (`Server/`) — FastAPI + SQLite on a Raspberry Pi, exposed publicly via Tailscale Funnel.
- **Utils** (`Utils/`) — `moods.yaml` (mood source of truth), the mood-header generator, and the BLE Wi-Fi provisioning script.
- **Docs** (`Docs/`) — architecture and CI/CD notes.

## Moods

Colours and patterns are defined in [`Utils/moods.yaml`](Utils/moods.yaml) and generated into
`Firmware/include/moods.h`. Three **status** moods are shown by the lamp but are never
user-selectable: `IDLE` (warm-white breath), `BLE` (cyan blink), `NO_WIFI` (orange blink).

The user-selectable moods:

| # | Mood | Colour(s) | Pattern | Period |
|---|---|---|---|---|
| 1 | Excited | bright yellow | breath | 1000 ms |
| 2 | Happy | rainbow (7 colours) | fade | 1000 ms |
| 3 | Sad | dark blue | breath | 2000 ms |
| 4 | Upset | dark red | breath | 1000 ms |
| 5 | Anxious | dark purple → purple → violet → red | breath alternate | 200 ms |
| 6 | Deep Breaths | teal green | breath | 10000 ms |
| 7 | Love | dark pink → pink → bright pink → light purple | fade | 1000 ms |
| 8 | Heepy | bright pink | blink | 500 ms |
| 9 | Hungry | dark brownish yellow | breath | 1000 ms |
| 10 | Tired | red → orange → yellow | fade | 1000 ms |
| 11 | Working | warm orange | solid | — |

Patterns: `solid`, `blink`, `breath`, `alternate` (hard switch between colours), `fade` (smooth
cross-fade), and `breath_alternate` (one breath per colour, advancing to the next each cycle).

To change moods, edit `Utils/moods.yaml`, then run `python Utils/generate_moods.py` to
regenerate the firmware header.
