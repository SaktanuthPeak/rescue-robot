"""USB serial transport for the receiver-canbus Arduino.

The receiver emits one RB3 snapshot every 100 ms:
    RB3,motor_code,motor_alive,arm_code,arm_alive,voltage_mV,voltage_adc,flame_front,flame_right,flame_rear,flame_left,seq*CK

This transport owns the receiver USB port.  The ``receiver`` flame source subscribes
to it, so the robot and flame dashboards consume one decoded stream without opening
the same serial port twice.
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Callable, Literal

from loguru import logger

from ..core.config import Settings

STATUS_NAMES: dict[int, str] = {
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

LinkState = Literal["disabled", "connecting", "streaming", "disconnected"]

ReceiverSampleSink = Callable[["ReceiverSample"], None]
ReceiverStateSink = Callable[[LinkState], None]
ReceiverParseErrorSink = Callable[[], None]


@dataclass(frozen=True, slots=True)
class ReceiverSample:
    motor_code: int
    motor_alive: bool
    arm_code: int
    arm_alive: bool
    battery_millivolts: int
    battery_adc: int
    flame_front: int
    flame_right: int
    flame_rear: int
    flame_left: int
    flame_valid: bool
    sequence: int
    received_at: datetime


def parse_line(raw: bytes) -> ReceiverSample | None:
    """Decode one RB1/RB2/RB3 line and reject noise, malformed fields, or bad checksum."""
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

    checksum = 0
    for value in payload.encode("ascii"):
        checksum ^= value
    if checksum != expected:
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
        or sequence < 0
    ):
        return None

    return ReceiverSample(
        motor_code=motor_code,
        motor_alive=bool(motor_alive),
        arm_code=arm_code,
        arm_alive=bool(arm_alive),
        battery_millivolts=battery_millivolts,
        battery_adc=battery_adc,
        flame_front=flame_front,
        flame_right=flame_right,
        flame_rear=flame_rear,
        flame_left=flame_left,
        flame_valid=flame_valid,
        sequence=sequence,
        received_at=datetime.now(UTC),
    )


class ReceiverCanbusService:
    """Reconnectable, thread-backed USB serial reader with command writes."""

    def __init__(self) -> None:
        self._settings: Settings | None = None
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._serial = None
        self._lock = threading.Lock()
        self._latest: ReceiverSample | None = None
        self._state: LinkState = "disabled"
        self._last_frame_monotonic = 0.0
        self._parse_errors = 0
        self._last_command: str | None = None
        self._last_command_at: datetime | None = None
        self._listeners: set[
            tuple[ReceiverSampleSink, ReceiverStateSink | None, ReceiverParseErrorSink | None]
        ] = set()

    def subscribe(
        self,
        on_sample: ReceiverSampleSink,
        on_state: ReceiverStateSink | None = None,
        on_parse_error: ReceiverParseErrorSink | None = None,
    ) -> Callable[[], None]:
        """Subscribe to decoded samples without opening another serial port."""
        listener = (on_sample, on_state, on_parse_error)
        with self._lock:
            self._listeners.add(listener)

        def unsubscribe() -> None:
            with self._lock:
                self._listeners.discard(listener)

        return unsubscribe

    def _notify_state(self, state: LinkState) -> None:
        with self._lock:
            listeners = tuple(self._listeners)
        for _, on_state, _ in listeners:
            if on_state is not None:
                on_state(state)

    def _notify_sample(self, sample: ReceiverSample) -> None:
        with self._lock:
            listeners = tuple(self._listeners)
        for on_sample, _, _ in listeners:
            on_sample(sample)

    def _notify_parse_error(self) -> None:
        with self._lock:
            listeners = tuple(self._listeners)
        for _, _, on_parse_error in listeners:
            if on_parse_error is not None:
                on_parse_error()

    async def start(self, settings: Settings) -> None:
        if not settings.ROBOT_SERIAL_ENABLED or self._thread is not None:
            self._settings = settings
            if not settings.ROBOT_SERIAL_ENABLED:
                with self._lock:
                    self._state = "disabled"
                self._notify_state("disabled")
            return

        self._settings = settings
        self._stop.clear()
        with self._lock:
            self._state = "connecting"
        self._notify_state("connecting")
        self._thread = threading.Thread(
            target=self._run,
            name="receiver-canbus-serial",
            daemon=True,
        )
        self._thread.start()
        logger.info(
            f"receiver CAN serial started on {settings.ROBOT_SERIAL_PORT} @ "
            f"{settings.ROBOT_SERIAL_BAUDRATE}"
        )

    async def stop(self) -> None:
        if self._thread is None:
            return
        self._stop.set()
        with self._lock:
            port = self._serial
        if port is not None:
            try:
                port.close()
            except Exception:
                pass
        self._thread.join(timeout=2.0)
        self._thread = None
        with self._lock:
            self._state = "disabled"
            self._serial = None
        self._notify_state("disabled")

    def send_command(self, channel: str, code: int) -> bool:
        """Send a one-shot command; the firmware applies its own fail-safe."""
        command = f"CMD:{channel.upper()}:{code}\n".encode("ascii")
        with self._lock:
            port = self._serial
            if port is None or not port.is_open:
                return False
            try:
                port.write(command)
                port.flush()
            except Exception as exc:
                logger.warning(f"receiver command failed: {exc!r}")
                return False
            self._last_command = command.decode("ascii").strip()
            self._last_command_at = datetime.now(UTC)
            return True

    def status(self) -> dict[str, object]:
        settings = self._settings
        with self._lock:
            sample = self._latest
            state = self._state
            parse_errors = self._parse_errors
            last_command = self._last_command
            last_command_at = self._last_command_at

        age_ms = 0
        if self._last_frame_monotonic:
            age_ms = int(max(time.monotonic() - self._last_frame_monotonic, 0.0) * 1000)

        effective_state = state
        if state == "streaming" and (
            self._last_frame_monotonic == 0.0 or age_ms > 1500
        ):
            effective_state = "disconnected"

        return {
            "type": "robot_status",
            "port": settings.ROBOT_SERIAL_PORT if settings else "/dev/ttyACM0",
            "baudrate": settings.ROBOT_SERIAL_BAUDRATE if settings else 115200,
            "state": effective_state,
            "last_frame_age_ms": age_ms,
            "parse_errors": parse_errors,
            "sequence": sample.sequence if sample else 0,
            "motor_code": sample.motor_code if sample else -1,
            "motor_status": STATUS_NAMES.get(sample.motor_code, "NO_DATA") if sample else "NO_DATA",
            "motor_can_alive": sample.motor_alive if sample else False,
            "arm_code": sample.arm_code if sample else -1,
            "arm_status": STATUS_NAMES.get(sample.arm_code, "NO_DATA") if sample else "NO_DATA",
            "arm_can_alive": sample.arm_alive if sample else False,
            "battery_millivolts": sample.battery_millivolts if sample else 0,
            "battery_volts": round(sample.battery_millivolts / 1000, 3) if sample else 0.0,
            "battery_adc": sample.battery_adc if sample else 0,
            "last_command": last_command,
            "last_command_at": last_command_at,
        }

    def _run(self) -> None:
        try:
            import serial
        except ImportError:
            logger.error("pyserial is not installed; install the backend dependencies")
            with self._lock:
                self._state = "disconnected"
            self._notify_state("disconnected")
            return

        settings = self._settings
        if settings is None:
            return

        while not self._stop.is_set():
            port = None
            try:
                port = serial.Serial(
                    settings.ROBOT_SERIAL_PORT,
                    settings.ROBOT_SERIAL_BAUDRATE,
                    timeout=settings.ROBOT_SERIAL_TIMEOUT_S,
                )
                self._stop.wait(settings.ROBOT_SERIAL_BOOT_DELAY_S)
                port.reset_input_buffer()
                with self._lock:
                    self._serial = port
                    self._state = "streaming"

                while not self._stop.is_set():
                    line = port.readline()
                    if not line:
                        continue
                    sample = parse_line(line)
                    if sample is None:
                        if line.lstrip().startswith((b"RB1,", b"RB2,", b"RB3,")):
                            with self._lock:
                                self._parse_errors += 1
                            self._notify_parse_error()
                        continue
                    with self._lock:
                        self._latest = sample
                        self._last_frame_monotonic = time.monotonic()
                        self._state = "streaming"
                    self._notify_state("streaming")
                    self._notify_sample(sample)
            except Exception as exc:
                logger.warning(
                    f"receiver CAN serial error on {settings.ROBOT_SERIAL_PORT}: {exc!r}"
                )
                with self._lock:
                    self._state = "disconnected"
                self._notify_state("disconnected")
            finally:
                if port is not None:
                    try:
                        port.close()
                    except Exception:
                        pass
                with self._lock:
                    if self._serial is port:
                        self._serial = None

            if not self._stop.is_set():
                with self._lock:
                    self._state = "connecting"
                self._notify_state("connecting")
                self._stop.wait(settings.ROBOT_SERIAL_RECONNECT_S)


receiver_canbus_service = ReceiverCanbusService()
