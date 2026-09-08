#!/usr/bin/env python3
"""Read RB3 telemetry from firmware/receiver-canbus over USB serial.

The firmware emits:
    RB3,motor_code,motor_alive,arm_code,arm_alive,voltage_mV,voltage_adc,
    flame_front,flame_right,flame_rear,flame_left,seq*CK

Human-readable Arduino log lines are intentionally ignored.  This makes the
reader safe to use while the receiver is printing boot and CAN diagnostics.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, dataclass

import serial


STATUS_NAMES = {
    0: "STOP",
    1: "FORWARD",
    2: "BACKWARD",
    3: "LEFT",
    4: "RIGHT",
    5: "FORWARD_LEFT",
    6: "FORWARD_RIGHT",
    7: "BACKWARD_LEFT",
    8: "BACKWARD_RIGHT",
    9: "RELEASE",
    10: "CLAMP",
}


@dataclass(frozen=True)
class ReceiverTelemetry:
    motor_code: int
    motor_status: str
    motor_alive: bool
    arm_code: int
    arm_status: str
    arm_alive: bool
    battery_millivolts: int
    battery_volts: float
    battery_adc: int
    flame_front: int
    flame_right: int
    flame_rear: int
    flame_left: int
    flame_valid: bool
    sequence: int


def parse_line(raw: bytes) -> ReceiverTelemetry | None:
    """Parse one RB1/RB2/RB3 line and reject noise or a bad XOR checksum."""
    text = raw.decode("ascii", errors="ignore").strip()
    if not text.startswith(("RB1,", "RB2,", "RB3,")):
        return None

    payload, separator, checksum_text = text.partition("*")
    if not separator or len(checksum_text) != 2:
        return None

    try:
        expected = int(checksum_text, 16)
    except ValueError:
        return None

    actual = 0
    for value in payload.encode("ascii"):
        actual ^= value
    if actual != expected:
        return None

    fields = payload.split(",")
    if fields[0] == "RB1" and len(fields) != 6:
        return None
    if fields[0] == "RB2" and len(fields) != 8:
        return None
    if fields[0] == "RB3" and len(fields) != 12:
        return None

    try:
        motor_code = int(fields[1])
        motor_alive = int(fields[2])
        arm_code = int(fields[3])
        arm_alive = int(fields[4])
        if fields[0] == "RB2":
            battery_millivolts = int(fields[5])
            battery_adc = int(fields[6])
            flame_front = flame_right = flame_rear = flame_left = 0
            flame_valid = False
            sequence = int(fields[7])
        elif fields[0] == "RB3":
            battery_millivolts = int(fields[5])
            battery_adc = int(fields[6])
            flame_front = int(fields[7])
            flame_right = int(fields[8])
            flame_rear = int(fields[9])
            flame_left = int(fields[10])
            flame_valid = True
            sequence = int(fields[11])
        else:
            battery_millivolts = 0
            battery_adc = 0
            flame_front = flame_right = flame_rear = flame_left = 0
            flame_valid = False
            sequence = int(fields[5])
    except ValueError:
        return None

    if motor_code not in STATUS_NAMES and motor_code != -1:
        return None
    if arm_code not in STATUS_NAMES and arm_code != -1:
        return None
    if (
        motor_alive not in (0, 1)
        or arm_alive not in (0, 1)
        or battery_millivolts < 0
        or not 0 <= battery_adc <= 1023
        or not all(0 <= value <= 1023 for value in (flame_front, flame_right, flame_rear, flame_left))
    ):
        return None
    if sequence < 0:
        return None

    return ReceiverTelemetry(
        motor_code=motor_code,
        motor_status=STATUS_NAMES.get(motor_code, "NO_DATA"),
        motor_alive=bool(motor_alive),
        arm_code=arm_code,
        arm_status=STATUS_NAMES.get(arm_code, "NO_DATA"),
        arm_alive=bool(arm_alive),
        battery_millivolts=battery_millivolts,
        battery_volts=round(battery_millivolts / 1000, 3),
        battery_adc=battery_adc,
        flame_front=flame_front,
        flame_right=flame_right,
        flame_rear=flame_rear,
        flame_left=flame_left,
        flame_valid=flame_valid,
        sequence=sequence,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Display receiver-canbus USB telemetry")
    parser.add_argument("--port", default="/dev/ttyACM0", help="USB serial device")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--json", action="store_true", help="Print one JSON object per frame")
    args = parser.parse_args()

    try:
        port = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as exc:
        print(f"cannot open {args.port}: {exc}", file=sys.stderr)
        return 1

    print(f"Listening on {args.port} @ {args.baud}. Press Ctrl-C to stop.")
    try:
        while True:
            telemetry = parse_line(port.readline())
            if telemetry is None:
                continue

            if args.json:
                print(json.dumps(asdict(telemetry), separators=(",", ":")), flush=True)
            else:
                motor_link = "CAN OK" if telemetry.motor_alive else "CAN TIMEOUT"
                arm_link = "CAN OK" if telemetry.arm_alive else "CAN TIMEOUT"
                print(
                    f"seq={telemetry.sequence:>6} | "
                    f"BAT={telemetry.battery_volts:>5.2f} V | "
                    f"FLAME={telemetry.flame_front:>4},{telemetry.flame_right:>4},"
                    f"{telemetry.flame_rear:>4},{telemetry.flame_left:>4} | "
                    f"MOTOR={telemetry.motor_status:<14} ({motor_link}) | "
                    f"ARM={telemetry.arm_status:<14} ({arm_link})",
                    flush=True,
                )
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        port.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
