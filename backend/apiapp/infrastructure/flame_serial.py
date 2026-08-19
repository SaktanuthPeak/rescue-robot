"""Flame telemetry transport: FB1 line protocol, plus mock and serial sources.

Wire format emitted by the Arduino sketch in ``firmware/flame_telemetry``::

    FB1,<front>,<right>,<rear>,<left>,<status>,<seq>*<CK>\\n

Example: ``FB1,812,118,1010,990,OK,4137*7B``

* ``FB1`` is a magic prefix. Any line not starting with it is discarded -- this is the
  entire defence against bootloader noise on connect.
* Four raw 10-bit ADC readings in fixed order front, right, rear, left (pins A0..A3).
* ``status`` is device-reported wiring health, orthogonal to flame detection.
* ``seq`` is a uint16 frame counter that wraps, so dropped lines are countable.
* ``*<CK>`` is the XOR of every byte before the ``*``, two hex digits. A line with no
  checksum is accepted (hand-typed bring-up lines work); a mismatching one is rejected.

This module owns *decoding* only. Polarity and threshold interpretation is business
logic and lives in ``apiapp/modules/telemetry/use_case.py``.
"""

from __future__ import annotations

import asyncio
import math
import random
import re
import threading
from abc import ABC, abstractmethod
from contextlib import suppress
from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Callable, Literal

from loguru import logger

from ..core.config import Settings

DeviceStatus = Literal["OK", "WARN", "FAULT"]
LinkState = Literal["streaming", "connecting", "disconnected"]

SEQ_MODULUS = 65536

# The \d{1,4} bound rejects absurd values before int() ever runs.
_LINE_RE = re.compile(
    r"^FB1,(\d{1,4}),(\d{1,4}),(\d{1,4}),(\d{1,4}),(OK|WARN|FAULT),(\d{1,5})$"
)


@dataclass(frozen=True, slots=True)
class FlameSample:
    """One decoded frame. Raw ADC only -- no polarity applied yet."""

    front: int
    right: int
    rear: int
    left: int
    status: DeviceStatus
    seq: int
    received_at: datetime


def _checksum_ok(payload: str, checksum: str) -> bool:
    """XOR every byte of payload and compare against the two-hex-digit checksum."""
    try:
        expected = int(checksum, 16)
    except ValueError:
        return False
    actual = 0
    for byte in payload.encode("ascii", errors="ignore"):
        actual ^= byte
    return actual == expected


def parse_line(raw: bytes) -> FlameSample | None:
    """Decode one serial line. Returns None for noise, garbage, or bad checksums."""
    text = raw.decode("ascii", errors="ignore").strip()
    if not text:
        return None

    payload, separator, checksum = text.partition("*")
    if separator and not _checksum_ok(payload, checksum):
        return None

    match = _LINE_RE.match(payload)
    if match is None:
        return None

    front, right, rear, left = (int(match.group(i)) for i in (1, 2, 3, 4))
    status: DeviceStatus = match.group(5)  # type: ignore[assignment]
    seq = int(match.group(6))
    if seq >= SEQ_MODULUS:
        return None

    return FlameSample(
        front=front,
        right=right,
        rear=rear,
        left=left,
        status=status,
        seq=seq,
        received_at=datetime.now(UTC),
    )


Sink = Callable[[FlameSample], None]
StateSink = Callable[[LinkState], None]


class FlameSource(ABC):
    """A source of flame samples.

    ``sink`` is a *synchronous* callable that is always invoked on the event loop
    thread. That symmetry is what removes any need for a queue between source and hub.
    """

    kind: Literal["mock", "serial"]

    @abstractmethod
    async def start(
        self,
        sink: Sink,
        on_state: StateSink,
        on_parse_error: Callable[[], None] | None = None,
    ) -> None: ...

    @abstractmethod
    async def stop(self) -> None: ...


class MockFlameSource(FlameSource):
    """A virtual flame orbiting the robot, so the UI works with no hardware.

    Deliberate quirks, so the dashboard's rarely-hit branches are all reachable:

    * ``status`` flips to WARN for ~2s every ~30s.
    * Every 97th frame skips one ``seq`` value, so ``dropped_frames`` is non-zero.
      **This is intentional -- do not debug it as a bug.**

    Intensity is mapped to raw ADC *through the configured polarity*, so mock data
    exercises exactly the same threshold path as real hardware.
    """

    kind = "mock"

    ORBIT_PERIOD_S = 20.0
    BEAM_EXPONENT = 2.2
    BACKSCATTER = 0.03
    NOISE_SIGMA = 0.02

    def __init__(self, settings: Settings) -> None:
        self._interval_s = max(settings.TELEMETRY_MOCK_INTERVAL_MS, 10) / 1000.0
        self._adc_max = settings.FLAME_ADC_MAX
        self._active_low = settings.FLAME_ACTIVE_LOW
        self._task: asyncio.Task[None] | None = None
        self._seq = 0
        self._elapsed_s = 0.0
        self._rng = random.Random(0xF1A3)
        # Small fixed per-sensor offsets so it does not look synthetically symmetric.
        self._offsets = (0.012, -0.008, 0.019, -0.004)

    async def start(
        self,
        sink: Sink,
        on_state: StateSink,
        on_parse_error: Callable[[], None] | None = None,
    ) -> None:
        if self._task is not None:
            return
        on_state("streaming")
        self._task = asyncio.create_task(self._run(sink), name="mock-flame-source")
        logger.info(
            f"mock flame source started ({self._interval_s * 1000:.0f}ms interval)"
        )

    async def stop(self) -> None:
        if self._task is None:
            return
        self._task.cancel()
        with suppress(asyncio.CancelledError):
            await self._task
        self._task = None
        logger.info("mock flame source stopped")

    async def _run(self, sink: Sink) -> None:
        while True:
            await asyncio.sleep(self._interval_s)
            self._elapsed_s += self._interval_s
            sink(self._next_sample())

    def _intensity_to_raw(self, intensity: float) -> int:
        """Map 0..1 intensity to a raw ADC reading through the configured polarity."""
        clamped = min(max(intensity, 0.0), 1.0)
        raw = (1.0 - clamped) * self._adc_max if self._active_low else clamped * self._adc_max
        return int(round(min(max(raw, 0), self._adc_max)))

    def _next_sample(self) -> FlameSample:
        t = self._elapsed_s
        bearing = (t / self.ORBIT_PERIOD_S) * 2.0 * math.pi
        # Source strength oscillates slowly so intensities are not a flat ring.
        strength = 0.55 + 0.42 * math.sin(t * 0.31)

        raws: list[int] = []
        for index, offset in enumerate(self._offsets):
            sensor_bearing = index * (math.pi / 2.0)
            delta = math.cos(bearing - sensor_bearing)
            lobe = max(delta, 0.0) ** self.BEAM_EXPONENT if delta > 0 else 0.0
            intensity = strength * lobe if lobe > 0 else self.BACKSCATTER
            intensity += offset + self._rng.gauss(0.0, self.NOISE_SIGMA)
            raws.append(self._intensity_to_raw(intensity))

        self._seq = (self._seq + 1) % SEQ_MODULUS
        if self._seq % 97 == 0:
            # Intentional gap -- see class docstring.
            self._seq = (self._seq + 1) % SEQ_MODULUS

        status: DeviceStatus = "WARN" if (t % 30.0) < 2.0 else "OK"

        return FlameSample(
            front=raws[0],
            right=raws[1],
            rear=raws[2],
            left=raws[3],
            status=status,
            seq=self._seq,
            received_at=datetime.now(UTC),
        )


class SerialFlameSource(FlameSource):
    """Reads FB1 frames from a USB serial port on a dedicated daemon thread.

    A dedicated thread rather than ``asyncio.to_thread``: ``to_thread`` runs on the
    default executor and is *not cancellable*, so a blocking ``readline()`` on a wedged
    port would hold a pool thread forever and can hang interpreter exit. One thread with
    a ``threading.Event`` and a 1s read timeout gives bounded, prompt shutdown.

    The read loop never raises into the app -- a missing port or a PermissionError
    degrades to a reconnect loop instead of killing startup.
    """

    kind = "serial"

    def __init__(self, settings: Settings) -> None:
        self._port_name = settings.FLAME_SERIAL_PORT
        self._baudrate = settings.FLAME_SERIAL_BAUDRATE
        self._timeout_s = settings.FLAME_SERIAL_TIMEOUT_S
        self._boot_delay_s = settings.FLAME_SERIAL_BOOT_DELAY_S
        self._reconnect_s = settings.FLAME_SERIAL_RECONNECT_S
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._loop: asyncio.AbstractEventLoop | None = None
        self._sink: Sink | None = None
        self._on_state: StateSink | None = None
        self._on_parse_error: Callable[[], None] | None = None

    async def start(
        self,
        sink: Sink,
        on_state: StateSink,
        on_parse_error: Callable[[], None] | None = None,
    ) -> None:
        if self._thread is not None:
            return
        self._loop = asyncio.get_running_loop()
        self._sink = sink
        self._on_state = on_state
        self._on_parse_error = on_parse_error
        self._stop.clear()
        on_state("connecting")
        self._thread = threading.Thread(
            target=self._run, name="serial-flame-source", daemon=True
        )
        self._thread.start()
        logger.info(
            f"serial flame source started on {self._port_name} @ {self._baudrate}"
        )

    async def stop(self) -> None:
        if self._thread is None:
            return
        self._stop.set()
        # to_thread IS right here: a bounded join, unlike the unbounded read loop.
        await asyncio.to_thread(self._thread.join, self._timeout_s + 1.0)
        if self._thread.is_alive():
            logger.warning("serial flame thread did not exit within the join timeout")
        self._thread = None
        logger.info("serial flame source stopped")

    def _post(self, fn: Callable[..., None], *args: object) -> bool:
        """Hand a callback to the event loop thread. False means the loop is gone."""
        if self._loop is None:
            return False
        try:
            self._loop.call_soon_threadsafe(fn, *args)
        except RuntimeError:
            # Loop already closed during a shutdown race.
            return False
        return True

    def _run(self) -> None:
        try:
            # Guarded import: a top-level `import serial` would be swallowed by the
            # bare `except ImportError: continue` in core/router.py, silently deleting
            # this module's routes (and, alphabetically, the ones after it).
            import serial
        except ImportError:
            logger.error("pyserial is not installed; run `poetry add pyserial`")
            if self._on_state:
                self._post(self._on_state, "disconnected")
            return

        while not self._stop.is_set():
            port = None
            try:
                port = serial.Serial(
                    self._port_name, self._baudrate, timeout=self._timeout_s
                )
                # An Uno R3 auto-resets when DTR is asserted; wait out the bootloader
                # and discard whatever it printed.
                self._stop.wait(self._boot_delay_s)
                port.reset_input_buffer()
                if self._on_state:
                    self._post(self._on_state, "streaming")

                while not self._stop.is_set():
                    line = port.readline()
                    if not line:
                        continue  # read timeout -- loop back and re-check the stop flag
                    sample = parse_line(line)
                    if sample is not None:
                        if self._sink and not self._post(self._sink, sample):
                            return
                    elif self._on_parse_error:
                        if not self._post(self._on_parse_error):
                            return
            except Exception as exc:
                logger.warning(f"flame serial error on {self._port_name}: {exc!r}")
                if self._on_state:
                    self._post(self._on_state, "disconnected")
            finally:
                if port is not None:
                    with suppress(Exception):
                        port.close()

            if not self._stop.is_set():
                # wait(), not sleep(), so shutdown during backoff is instant.
                self._stop.wait(self._reconnect_s)


def create_flame_source(settings: Settings) -> FlameSource:
    """Pick the transport named by TELEMETRY_SOURCE."""
    if settings.TELEMETRY_SOURCE == "serial":
        return SerialFlameSource(settings)
    return MockFlameSource(settings)
