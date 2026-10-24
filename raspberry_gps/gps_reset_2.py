"""
Send UBX commands to reset the GPS module and configure GNSS signal systems.
This script performs a cold start and observes NMEA output for a specified duration.

It's specifically for the Beitian BE-220 GPS module.
"""
import struct
import time

import serial


PORT = "/dev/ttyAMA2"
BAUD = 38400
OBSERVE_SECONDS = 180


def ubx(msg_class: int, msg_id: int, payload: bytes) -> bytes:
    body = bytes([msg_class, msg_id])
    body += struct.pack("<H", len(payload)) + payload

    a = b = 0
    for byte in body:
        a = (a + byte) & 0xFF
        b = (b + a) & 0xFF

    return b"\xb5\x62" + body + bytes([a, b])


def wait_for_ack(ser, target_class, target_id, timeout=3.0):
    """Look for UBX-ACK-ACK or UBX-ACK-NAK amid NMEA output."""
    received = bytearray()
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            received.extend(chunk)

        for ack_id, result in ((0x01, "ACK"), (0x00, "NAK")):
            marker = (
                b"\xb5\x62\x05"
                + bytes([ack_id, 0x02, 0x00, target_class, target_id])
            )
            if marker in received:
                return result

        # Keep memory bounded while preserving partial frames.
        if len(received) > 8192:
            del received[:-16]

    return "NO ACK"


# Enable the signal systems supported by the BE-220.
# Each key is a u-blox M10 CFG-SIGNAL configuration item.
SIGNALS = [
    (0x1031001F, "GPS"),
    (0x10310001, "GPS L1 C/A"),
    (0x10310021, "Galileo"),
    (0x10310007, "Galileo E1"),
    (0x10310022, "BeiDou"),
    (0x1031000D, "BeiDou B1I"),
    (0x1031000F, "BeiDou B1C"),
    (0x10310024, "QZSS"),
    (0x10310012, "QZSS L1 C/A"),
    (0x10310014, "QZSS L1S"),
    (0x10310020, "SBAS"),
    (0x10310005, "SBAS L1 C/A"),
]


with serial.Serial(PORT, BAUD, timeout=0.1, write_timeout=2) as ser:
    print(f"Connected to {PORT} at {BAUD} baud")

    # CFG-VALSET: version 0, RAM + BBR + Flash, no transaction.
    payload = bytes([0x00, 0x07, 0x00, 0x00])
    payload += b"".join(
        struct.pack("<I", key) + b"\x01"
        for key, _name in SIGNALS
    )

    print("\nEnabling GNSS systems...")
    ser.write(ubx(0x06, 0x8A, payload))
    ser.flush()

    result = wait_for_ack(ser, 0x06, 0x8A)
    print(f"Configuration result: {result}")

    for _key, name in SIGNALS:
        print(f"  Enable requested: {name}")

    # Let the GNSS subsystem finish applying its configuration.
    time.sleep(1)

    # CFG-RST: clear navigation data and perform a cold start.
    # 0xFFFF = cold start; 0x02 = controlled GNSS-only reset.
    reset_payload = struct.pack("<HBB", 0xFFFF, 0x02, 0x00)

    print("\nRequesting GNSS cold start...")
    ser.write(ubx(0x06, 0x04, reset_payload))
    ser.flush()

    # CFG-RST is not expected to be acknowledged.
    time.sleep(2)

    print(f"\nMonitoring NMEA for {OBSERVE_SECONDS} seconds...")
    print("Watch GSV for satellites in view and their signal levels.")
    print("Watch GSA/GGA for satellites used and fix quality.\n")

    deadline = time.monotonic() + OBSERVE_SECONDS

    while time.monotonic() < deadline:
        line = ser.readline().strip()

        if not line.startswith(b"$") or b"," not in line:
            continue

        try:
            sentence = line.decode("ascii")
        except UnicodeDecodeError:
            continue

        header = sentence[1:].split(",", 1)[0]
        sentence_type = header[-3:]

        if sentence_type in {"GGA", "GSA", "GSV", "GNS", "RMC"}:
            print(sentence)
