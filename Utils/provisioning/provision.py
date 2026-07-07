#!/usr/bin/env python3
"""
Send Wi-Fi credentials to a Mood Lamp over BLE — terminal only, no UI.

Setup:
    python3 -m venv .venv && source .venv/bin/activate
    pip install bleak

Usage:
    1. Put the lamp in provisioning mode (LED blinking cyan).
    2. Run:  python3 Utils/provisioning/provision.py
    3. Enter the Wi-Fi SSID and password when prompted.

The UUIDs and device name below must match Firmware/src/net/ble.cpp.
"""
import asyncio

from bleak import BleakScanner, BleakClient

DEVICE_NAME = "MoodLamp"
SVC_UUID    = "9a1e0000-8f4a-4b1c-9e2a-1234567890ab"
SSID_UUID   = "9a1e0001-8f4a-4b1c-9e2a-1234567890ab"
PASS_UUID   = "9a1e0002-8f4a-4b1c-9e2a-1234567890ab"
APPLY_UUID  = "9a1e0003-8f4a-4b1c-9e2a-1234567890ab"
STATUS_UUID = "9a1e0004-8f4a-4b1c-9e2a-1234567890ab"


async def main():
    ssid = input("Wi-Fi SSID: ").strip()
    password = input("Wi-Fi password: ").strip()
    if not ssid:
        print("SSID cannot be empty.")
        return

    print(f"Scanning for '{DEVICE_NAME}' (the lamp LED should be blinking cyan)...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=20.0)
    if device is None:
        print("Lamp not found. Is it in provisioning mode? Try again.")
        return

    async with BleakClient(device) as client:
        print("Connected. Sending credentials...")

        got_status = asyncio.Event()

        def on_status(_, data: bytearray):
            print("Lamp status:", data.decode(errors="replace"))
            got_status.set()

        try:
            await client.start_notify(STATUS_UUID, on_status)
        except Exception:
            pass

        await client.write_gatt_char(SSID_UUID, ssid.encode(), response=True)
        await client.write_gatt_char(PASS_UUID, password.encode(), response=True)
        await client.write_gatt_char(APPLY_UUID, b"\x01", response=True)
        print("Credentials sent. Waiting for the lamp to connect to Wi-Fi...")

        try:
            await asyncio.wait_for(got_status.wait(), timeout=15.0)
        except asyncio.TimeoutError:
            print("No status received (the lamp may have already switched to Wi-Fi).")

    print("\nWatch the lamp's LED:")
    print("  - animated mood colour  -> connected, provisioning done")
    print("  - red blinking          -> Wi-Fi failed, run this again")


if __name__ == "__main__":
    asyncio.run(main())
