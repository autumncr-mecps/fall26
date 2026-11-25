import asyncio
import struct
from bleak import BleakClient

DEVICE = "00:70:07:a2:1b:a6"
CHARACTERISTIC = "2A37"
connected = False

async def main():
    last_val = 0
    async with BleakClient(DEVICE) as client:
        print("found client")
        while True:
            dev_value = await client.read_gatt_char(CHARACTERISTIC)
            dev_value = struct.unpack('f', dev_value)
            if last_val != dev_value:
                print(f"Heart rate: {dev_value}")
            last_val = dev_value;

asyncio.run(main())
