"""
Send UBX commands to reset the GPS module at various baud rates.
This script attempts a factory reset followed by a hardware reset.

It's specifically for the Beitian BE-220 GPS module.
"""

import struct
import time
import serial

PORT = "/dev/ttyAMA2"
BAUDS = [
    4800, 9600, 19200, 38400, 57600,
    115200, 230400, 460800, 921600
]


def ubx(msg_class, msg_id, payload):
    body = bytes([msg_class, msg_id])
    body += struct.pack("<H", len(payload)) + payload

    a = b = 0
    for byte in body:
        a = (a + byte) & 0xFF
        b = (b + a) & 0xFF

    return b"\xb5\x62" + body + bytes([a, b])


# Clear saved configuration and reload defaults.
factory_reset = ubx(
    0x06, 0x09,
    struct.pack("<III", 0xFFFFFFFF, 0, 0xFFFFFFFF)
)

# Clear navigation data and immediately reset via watchdog.
hardware_reset = ubx(
    0x06, 0x04,
    struct.pack("<HBB", 0xFFFF, 0x00, 0x00)
)


def send_at_all_bauds(packet, description):
    for baud in BAUDS:
        try:
            with serial.Serial(
                PORT, baudrate=baud, timeout=0.1,
                write_timeout=1
            ) as ser:
                ser.write(packet)
                ser.flush()

            print(f"{description}: sent at {baud}")
            time.sleep(0.2)

        except (serial.SerialException, OSError) as exc:
            print(f"{baud}: {exc}")


send_at_all_bauds(factory_reset, "Factory reset")
time.sleep(1)
send_at_all_bauds(hardware_reset, "Watchdog reset")

print("Commands sent; success is not confirmed.")
