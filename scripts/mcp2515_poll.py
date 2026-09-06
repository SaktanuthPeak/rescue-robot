#!/usr/bin/env python3
"""Poll an MCP2515 over SPI without using its INT pin.

This is a small standalone SocketCAN-free test program for Raspberry Pi.
It is intended for low-rate traffic such as the project's Arduino test sender.

The Raspberry Pi must expose the MCP2515 as /dev/spidev0.0.  Do not load the
``mcp2515-can0`` device-tree overlay at the same time: that overlay binds the
kernel mcp251x driver to the chip and prevents this program from opening it.
"""

from __future__ import annotations

import argparse
import signal
import sys
import time
from dataclasses import dataclass

import spidev


# MCP2515 SPI instructions
RESET = 0xC0
READ = 0x03
WRITE = 0x02
BIT_MODIFY = 0x05
READ_RX_BUFFER_0 = 0x90
READ_RX_BUFFER_1 = 0x94

# MCP2515 registers
CANSTAT = 0x0E
CANCTRL = 0x0F
CNF3 = 0x28
CNF2 = 0x29
CNF1 = 0x2A
CANINTE = 0x2B
CANINTF = 0x2C
EFLG = 0x2D
RXB0CTRL = 0x60
RXB1CTRL = 0x70

# CANINTF / EFLG bits
RX0IF = 0x01
RX1IF = 0x02
RX0OVR = 0x40
RX1OVR = 0x80

# CANCTRL/CANSTAT operation modes
MODE_NORMAL = 0x00
MODE_SLEEP = 0x20
MODE_LOOPBACK = 0x40
MODE_LISTEN_ONLY = 0x60
MODE_CONFIG = 0x80


@dataclass(frozen=True)
class Timing:
    cnf1: int
    cnf2: int
    cnf3: int
    sample_point: float


@dataclass(frozen=True)
class CanFrame:
    can_id: int
    data: bytes
    extended: bool
    timestamp: float


def calculate_timing(clock_hz: int, bitrate: int) -> Timing:
    """Find valid MCP2515 bit timing for the requested clock and bitrate."""
    candidates: list[tuple[float, float, Timing]] = []

    for brp in range(64):
        for total_tq in range(8, 26):
            actual = clock_hz / (2 * (brp + 1) * total_tq)
            if abs(actual - bitrate) > 0.5:
                continue

            for prop_seg in range(1, 9):
                for phase_seg1 in range(1, 9):
                    phase_seg2 = total_tq - 1 - prop_seg - phase_seg1
                    if not 2 <= phase_seg2 <= 8:
                        continue

                    sample_point = (
                        1 + prop_seg + phase_seg1
                    ) / total_tq
                    # Prefer a sample point near 87.5%, then a larger TQ count.
                    score = abs(sample_point - 0.875) + (1 / total_tq) * 0.001

                    cnf1 = (brp & 0x3F) | (0 << 6)  # SJW = 1 TQ
                    cnf2 = (
                        0x80  # BTLMODE: PHSEG2 from CNF3
                        | ((phase_seg1 - 1) << 3)
                        | (prop_seg - 1)
                    )
                    cnf3 = phase_seg2 - 1
                    candidates.append(
                        (
                            score,
                            -total_tq,
                            Timing(cnf1, cnf2, cnf3, sample_point),
                        )
                    )

    if not candidates:
        raise ValueError(
            f"Cannot generate exact MCP2515 timing for {clock_hz} Hz/{bitrate} bit/s"
        )

    candidates.sort(key=lambda item: (item[0], item[1]))
    return candidates[0][2]


class MCP2515:
    def __init__(self, device: str, spi_hz: int, oscillator_hz: int, bitrate: int):
        self.spi = spidev.SpiDev()
        try:
            bus_cs = device.removeprefix("/dev/spidev").split(".")
            if len(bus_cs) != 2:
                raise ValueError
            bus, chip_select = (int(value) for value in bus_cs)
        except ValueError as exc:
            raise ValueError(
                f"SPI device must look like /dev/spidev0.0, got {device!r}"
            ) from exc

        self.spi.open(bus, chip_select)
        self.spi.max_speed_hz = spi_hz
        self.spi.mode = 0
        self.spi.bits_per_word = 8
        self.timing = calculate_timing(oscillator_hz, bitrate)

    def close(self) -> None:
        self.spi.close()

    def reset(self) -> None:
        self.spi.xfer2([RESET])
        time.sleep(0.01)

    def read_register(self, address: int) -> int:
        return self.spi.xfer2([READ, address & 0x7F, 0x00])[2]

    def write_register(self, address: int, value: int) -> None:
        self.spi.xfer2([WRITE, address & 0x7F, value & 0xFF])

    def write_registers(self, address: int, values: list[int]) -> None:
        self.spi.xfer2([WRITE, address & 0x7F, *[v & 0xFF for v in values]])

    def bit_modify(self, address: int, mask: int, value: int) -> None:
        self.spi.xfer2(
            [BIT_MODIFY, address & 0x7F, mask & 0xFF, value & 0xFF]
        )

    def set_mode(self, mode: int) -> None:
        self.bit_modify(CANCTRL, 0xE0, mode)
        deadline = time.monotonic() + 0.1
        last_status = 0xFF
        while time.monotonic() < deadline:
            last_status = self.read_register(CANSTAT)
            if last_status & 0xE0 == mode:
                return
            time.sleep(0.001)
        raise RuntimeError(
            f"MCP2515 did not enter mode 0x{mode:02X}; "
            f"last CANSTAT=0x{last_status:02X}"
        )

    def initialize(self) -> None:
        self.reset()
        reset_canstat = self.read_register(CANSTAT)
        reset_canctrl = self.read_register(CANCTRL)
        print(
            f"After reset: CANSTAT=0x{reset_canstat:02X} "
            f"CANCTRL=0x{reset_canctrl:02X}",
            flush=True,
        )

        # MCP2515 normally starts in configuration mode after RESET.
        # Avoid a redundant BIT MODIFY when it is already there, while still
        # failing with the raw register value when SPI wiring is wrong.
        if reset_canstat & 0xE0 != MODE_CONFIG:
            self.set_mode(MODE_CONFIG)

        self.write_registers(
            CNF1,
            [self.timing.cnf1, self.timing.cnf2, self.timing.cnf3],
        )

        # RXM=11 accepts all standard and extended frames; BUKT enables
        # rollover from RXB0 into RXB1.  No hardware interrupt is used.
        self.write_register(RXB0CTRL, 0x64)
        self.write_register(RXB1CTRL, 0x60)
        self.write_register(CANINTE, 0x00)
        self.write_register(CANINTF, 0x00)
        self.write_register(EFLG, 0x00)

        self.set_mode(MODE_NORMAL)

    def _read_rx_buffer(self, instruction: int) -> CanFrame:
        # SIDH, SIDL, EID8, EID0, DLC, DATA0..DATA7
        values = self.spi.xfer2([instruction, *([0x00] * 13)])[1:]
        sidh, sidl, eid8, eid0, dlc = values[:5]
        extended = bool(sidl & 0x08)

        if extended:
            can_id = (
                (sidh << 21)
                | ((sidl & 0xE0) << 13)
                | ((sidl & 0x03) << 16)
                | (eid8 << 8)
                | eid0
            )
        else:
            can_id = (sidh << 3) | (sidl >> 5)

        dlc &= 0x0F
        if dlc > 8:
            dlc = 8

        return CanFrame(
            can_id=can_id,
            data=bytes(values[5 : 5 + dlc]),
            extended=extended,
            timestamp=time.time(),
        )

    def poll(self) -> list[CanFrame]:
        """Read all frames currently waiting in MCP2515 RX buffers."""
        flags = self.read_register(CANINTF)
        frames: list[CanFrame] = []

        # Read both buffers.  RXB0 rollover can set RX1IF as well.
        if flags & RX0IF:
            frames.append(self._read_rx_buffer(READ_RX_BUFFER_0))
            self.bit_modify(CANINTF, RX0IF, 0x00)

        if flags & RX1IF:
            frames.append(self._read_rx_buffer(READ_RX_BUFFER_1))
            self.bit_modify(CANINTF, RX1IF, 0x00)

        error_flags = self.read_register(EFLG)
        if error_flags & (RX0OVR | RX1OVR):
            print(
                f"WARNING: MCP2515 RX overflow, EFLG=0x{error_flags:02X}",
                file=sys.stderr,
            )
            self.bit_modify(EFLG, RX0OVR | RX1OVR, 0x00)

        return frames


def format_frame(frame: CanFrame) -> str:
    frame_type = "EXT" if frame.extended else "STD"
    payload = frame.data.hex(" ").upper()
    label = {0x100: "MOTOR", 0x101: "ARM"}.get(frame.can_id, "")
    label_text = f" {label}" if label else ""
    return (
        f"{frame.timestamp:.3f} ID=0x{frame.can_id:X} {frame_type}"
        f" DLC={len(frame.data)} DATA=[{payload}]{label_text}"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Poll MCP2515 CAN frames over SPI without INT"
    )
    parser.add_argument("--device", default="/dev/spidev0.0")
    parser.add_argument("--spi-hz", type=int, default=1_000_000)
    parser.add_argument("--oscillator", type=int, default=8_000_000)
    parser.add_argument("--bitrate", type=int, default=500_000)
    parser.add_argument("--poll-ms", type=float, default=1.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.poll_ms <= 0:
        raise SystemExit("--poll-ms must be greater than zero")

    print(f"Opening SPI device: {args.device}", flush=True)
    controller = MCP2515(
        device=args.device,
        spi_hz=args.spi_hz,
        oscillator_hz=args.oscillator,
        bitrate=args.bitrate,
    )
    stop = False

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    try:
        print("Initializing MCP2515 (polling mode, no INT)...", flush=True)
        controller.initialize()
        timing = controller.timing
        print(
            "MCP2515 polling receiver ready: "
            f"device={args.device} bitrate={args.bitrate} "
            f"CNF=({timing.cnf1:02X},{timing.cnf2:02X},{timing.cnf3:02X}) "
            f"sample_point={timing.sample_point:.3f}"
        )

        delay = args.poll_ms / 1000.0
        last_waiting_report = time.monotonic()
        while not stop:
            for frame in controller.poll():
                print(format_frame(frame), flush=True)

            if time.monotonic() - last_waiting_report >= 5.0:
                print("Waiting for CAN frames...", flush=True)
                last_waiting_report = time.monotonic()

            time.sleep(delay)
    finally:
        controller.close()

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
