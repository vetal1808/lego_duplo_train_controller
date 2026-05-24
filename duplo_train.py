"""
LEGO DUPLO Train Controller
Control the train via Bluetooth Low Energy
Requirements: pip install bleak
"""

import asyncio
import sys
import struct
from bleak import BleakScanner, BleakClient

CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123"
#0F 00 04 33 01 5B 00 01 00 00 00 01 00 00 00 
#0F 00 04 35 01 14 00 01 00 00 00 01 00 00 00 
#0F 00 04 36 01 2C 00 01 00 00 00 01 00 00 00 
#0F 00 04 34 01 5A 00 01 00 00 00 01 00 00 00 

#0F 00 04 32 01 29 00 01 00 00 00 01 00 00 00 
#0F 00 04 33 01 5B 00 01 00 00 00 01 00 00 00 
#0F 00 04 35 01 14 00 01 00 00 00 01 00 00 00 
#0F 00 04 36 01 2C 00 01 00 00 00 01 00 00 00 
#0F 00 04 34 01 5A 00 01 00 00 00 01 00 00 00 

PORT_MOTOR = 0x32
PORT_COLOR = 0x33
PORT_BATTERY = 0x35
PORT_SPEED_SENSOR = 0x36

TAG_COLORS = {
    0x01: "Yellow",
    0x02: "Green",
    0x03: "Blue",
    0x05: "Red",
    0x0A: "White",
#manualy identified tags
    0x77: "Green (tree)", 
    0x1C: "Green (spark)",
    0x92: "Pink (home)"
}

class DuploTrain:
    def __init__(self):
        self.client = None
        self.speed = 0
        self.color = "No tile"
        self.accel = 0

    async def connect(self):
        print("🔍 Searching for DUPLO Train...")
        devices = await BleakScanner.discover(timeout=5.0)

        target = None
        for d in devices:
            name = d.name or ""
            if "DUPLO" in name or "Train" in name or "LEGO" in name:
                target = d
                print(f"✅ Found: {d.name} [{d.address}]")
                break

        if not target:
            print("❌ Train not found. Make sure the hub is on and blinking.")
            return False

        self.client = BleakClient(target.address)
        await self.client.connect()
        print("🚂 Connected!")

        await self.client.start_notify(CHARACTERISTIC_UUID, self._on_notify)

        # Subscribe to color sensor
        await self.send(bytes([0x0A, 0x00, 0x41, PORT_COLOR, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01]))
        # Subscribe to accelerometer
        #await self.send(bytes([0x0A, 0x00, 0x41, PORT_ACCEL, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01]))
        # Subcribe to speed sensor
        await self.send(bytes([0x0A, 0x00, 0x41, PORT_SPEED_SENSOR, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01]))
        # Subscribe to battery level
        await self.send(bytes([0x0A, 0x00, 0x41, PORT_BATTERY, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01]))
        return True

    def _on_notify(self, sender, data):
        if len(data) < 5:
            return
        msg_type = data[2]
        port = data[3]

        if msg_type == 0x45:
            if port == PORT_COLOR and len(data) >= 5:
                color_id = data[4]
                name = TAG_COLORS.get(color_id, f"Unknown (0x{color_id:02X})")
                self.color = name
                print(f"🎨 Tile color: {name}")

            elif port == PORT_SPEED_SENSOR and len(data) >= 5:
                speed_value = data[4]
                #TODO add speed convertion to signed value
                print(f"🏃 Speedometer: {speed_value} (0x{speed_value:02X})")

            elif port == PORT_BATTERY and len(data) >= 6:
                voltage_mv = struct.unpack_from("<H", data, 4)[0]
                print(f"🔋 Battery: {voltage_mv} mV ")

    async def send(self, data: bytes):
        if self.client and self.client.is_connected:
            await self.client.write_gatt_char(CHARACTERISTIC_UUID, data, response=False)

    async def set_speed(self, speed: int):
        """Speed from -100 to 100"""
        speed = max(-100, min(100, speed))
        self.speed = speed

        if speed == 0:
            s = 0
            await self.stop()
        else:
            if speed >= 0:
                s = speed          # 0..100
            else:
                s = 256 + speed    # -1..-100 => 255..156
            cmd = bytes([0x09, 0x00, 0x81, PORT_MOTOR, 0x11, 0x07, s, 0x64, 0x03])
            await self.send(cmd)
        print(f"🚂 Speed: {speed}%")

    async def stop(self):
        """Stop — float mode (7F)"""
        self.speed = 0
        cmd = bytes([0x08, 0x00, 0x81, PORT_MOTOR, 0x11, 0x51, 0x00, 0x7F])
        await self.send(cmd)
        print("⛔ Stop")

    async def disconnect(self):
        if self.client:
            await self.stop()
            await asyncio.sleep(0.3)
            await self.client.disconnect()
            print("👋 Disconnected")


async def keyboard_control(train: DuploTrain):
    import msvcrt  # Windows only

    print("\n" + "="*40)
    print("  🚂 DUPLO Train Controller")
    print("="*40)
    print("  ↑  Up arrow       — faster (+20%)")
    print("  ↓  Down arrow     — slower (-20%)")
    print("  SPACE             — stop")
    print("  Q                 — quit")
    print("="*40 + "\n")

    speed = 0
    step = 20

    while True:
        if msvcrt.kbhit():
            key = msvcrt.getch()

            if key == b'\xe0':
                key2 = msvcrt.getch()
                if key2 == b'H':    # Up arrow
                    speed = min(100, speed + step)
                    await train.set_speed(speed)
                elif key2 == b'P':  # Down arrow
                    speed = max(-100, speed - step)
                    await train.set_speed(speed)

            elif key == b' ':
                speed = 0
                await train.stop()

            elif key in (b'q', b'Q'):
                print("Exiting...")
                break

        await asyncio.sleep(0.05)


async def main():
    train = DuploTrain()

    connected = await train.connect()
    if not connected:
        sys.exit(1)

    try:
        await keyboard_control(train)
    finally:
        await train.disconnect()


if __name__ == "__main__":
    asyncio.run(main())