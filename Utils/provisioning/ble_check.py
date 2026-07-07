import asyncio
from bleak import BleakScanner

async def main():
    print("Scanning 10s...")
    found = await BleakScanner.discover(timeout=10.0, return_adv=True)
    for addr, (dev, adv) in found.items():
        print(f"{addr}  name={adv.local_name!r}  uuids={adv.service_uuids}")

asyncio.run(main())