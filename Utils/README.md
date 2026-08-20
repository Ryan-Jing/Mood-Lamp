# Utils

Host-side tooling: the mood definitions and their code generator, plus the BLE Wi-Fi
provisioning script.

| File | Purpose |
|---|---|
| `moods.yaml` | Single source of truth for every mood's colour(s), pattern, and period |
| `generate_moods.py` | Bakes `moods.yaml` into `Firmware/include/moods.h` |
| `provision.py` | Sends Wi-Fi credentials to a lamp over BLE |
| `ble_check.py` | Lists nearby BLE devices — used to confirm a lamp is advertising |
| `Button-Test/` | Standalone PlatformIO sketch for bench-testing the dual-stage button |

Run every script from the repo root; they resolve paths relative to their own location.

---

## `provision.py`

Connects to a Mood Lamp over BLE and hands it a Wi-Fi SSID and password, so the lamp can
join the network and reach the server. It targets the GATT service in
`Firmware/src/net/ble.cpp` — device name `MoodLamp`, service
`9a1e0000-8f4a-4b1c-9e2a-1234567890ab`, with SSID / password / apply / status
characteristics. See [`Docs/architecture.md`](../Docs/architecture.md) for the full layout.

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

Put the lamp in provisioning mode first — it's already there on first boot (LED blinking
cyan), otherwise hold the button fully pressed for 5 seconds. Provisioning stays open for
2 minutes. Then:

```bash
python3 Utils/provision.py
```

Enter the SSID and password at the prompts. The lamp stores them in NVS, drops BLE, and
joins Wi-Fi; the script prints the lamp's status notification before exiting. A 10 s full
button hold erases the stored credentials and reopens provisioning.

If the scan times out, check that the lamp is advertising:

```bash
python3 Utils/ble_check.py
```

## `generate_moods.py`

Regenerates `Firmware/include/moods.h` from `moods.yaml`. That header is **auto-generated**
— never edit it by hand.

```bash
pip install pyyaml
```

```bash
python3 Utils/generate_moods.py
```

Re-flash both lamps afterwards: the enum index is the `mood_id` sent over the wire, so
lamps running different mood tables will render different moods. The mood chart lives in
the [root README](../README.md#moods).
