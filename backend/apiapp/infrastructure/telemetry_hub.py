"""Process-wide telemetry hub: owns the flame source and fans samples out to clients.

A module-level singleton (mirroring ``beanie_client`` in ``database.py``) because
``run.py``'s lifespan must start and stop it, and FastAPI's dependency injection is not
available inside ``lifespan``.
"""

from __future__ import annotations

import asyncio
import time
from contextlib import suppress
from dataclasses import dataclass
from datetime import UTC, datetime

from loguru import logger

from ..core.config import Settings
from .flame_serial import (
    SEQ_MODULUS,
    FlameSample,
    FlameSource,
    LinkState,
    create_flame_source,
)

# A seq jump larger than this is a board reset, not dropped frames.
_MAX_PLAUSIBLE_GAP = 1000
_WATCHDOG_TICK_S = 1.0


@dataclass
class LinkStats:
    source: str = "mock"
    state: LinkState = "connecting"
    dropped_frames: int = 0
    parse_errors: int = 0
    last_frame_age_ms: int = 0


class TelemetryHub:
    """Single source of flame samples, fanned out to N WebSocket clients.

    Fan-out is latest-wins: one ``Queue(maxsize=1)`` per client, and a full queue has its
    stale frame discarded. ``_fanout`` contains no ``await`` points at all, which is
    precisely why one dead or slow client can never stall the broadcast -- it only
    starves its own queue. Telemetry has no use for stale frames, so dropping them is
    correct rather than a compromise.
    """

    def __init__(self) -> None:
        self._source: FlameSource | None = None
        self._settings: Settings | None = None
        self._subscribers: set[asyncio.Queue[FlameSample]] = set()
        self._latest: FlameSample | None = None
        self._stats = LinkStats()
        self._prev_seq: int | None = None
        self._last_monotonic: float = 0.0
        self._watchdog: asyncio.Task[None] | None = None
        self._stale_after_s: float = 1.5
        self._loop: asyncio.AbstractEventLoop | None = None

    # ---- lifecycle -------------------------------------------------------------

    async def start(self, settings: Settings) -> None:
        """Start the source, or no-op if already running on this event loop.

        Idempotence matters because ``create_app()`` installs the lifespan twice. But it
        must be scoped to the *current* event loop: this is a process-wide singleton
        holding asyncio primitives (queues, tasks) that belong to whichever loop created
        them. A test suite building several apps -- or anything else running a second loop
        in one process -- would otherwise get an early return here and then await a queue
        that only ever gets fed from a loop that is no longer running, hanging forever.
        Rebinding is a no-op in production, where there is exactly one loop.
        """
        running_loop = asyncio.get_running_loop()
        if self._source is not None:
            if self._loop is running_loop:
                return
            logger.debug("telemetry hub rebinding to a new event loop")
            self._discard_stale_state()

        self._loop = running_loop
        self._settings = settings
        self._stale_after_s = max(settings.TELEMETRY_STALE_AFTER_MS, 100) / 1000.0
        self._prev_seq = None
        self._latest = self._idle_sample(settings)
        self._last_monotonic = time.monotonic()
        self._stats = LinkStats(source=settings.TELEMETRY_SOURCE, state="connecting")

        self._source = create_flame_source(settings)
        await self._source.start(self.publish, self._set_state, self._note_parse_error)
        self._watchdog = asyncio.create_task(
            self._run_watchdog(), name="telemetry-watchdog"
        )
        logger.info(f"telemetry hub started (source={settings.TELEMETRY_SOURCE})")

    def _discard_stale_state(self) -> None:
        """Drop primitives owned by a previous event loop, without awaiting them.

        Only reachable on the rebind path above. The old loop may already be closed, so
        its task cannot be awaited and ``cancel()`` may itself raise -- the references
        are dropped instead and left to the garbage collector. A serial reader thread
        outlives this, but its ``call_soon_threadsafe`` will fail against the dead loop
        and the thread exits on its own.
        """
        watchdog, self._watchdog = self._watchdog, None
        if watchdog is not None:
            with suppress(RuntimeError):
                watchdog.cancel()
        self._source = None
        self._subscribers.clear()
        self._latest = None
        self._prev_seq = None
        self._settings = None
        self._loop = None

    async def stop(self) -> None:
        """Full reset, so a later start() works cleanly in the same process."""
        if self._watchdog is not None:
            self._watchdog.cancel()
            with suppress(asyncio.CancelledError):
                await self._watchdog
            self._watchdog = None

        if self._source is not None:
            await self._source.stop()
            self._source = None

        self._subscribers.clear()
        self._latest = None
        self._prev_seq = None
        self._settings = None
        self._loop = None
        logger.info("telemetry hub stopped")

    @staticmethod
    def _idle_sample(settings: Settings) -> FlameSample:
        """All channels at the no-flame rail.

        Seeded at start() so the frame shape is invariant from the very first request:
        ``flame`` is never null and the REST snapshot never has to 404.
        """
        rail = settings.FLAME_ADC_MAX if settings.FLAME_ACTIVE_LOW else 0
        return FlameSample(
            front=rail,
            right=rail,
            rear=rail,
            left=rail,
            status="FAULT",
            seq=0,
            received_at=datetime.now(UTC),
        )

    # ---- publish / subscribe ---------------------------------------------------

    def publish(self, sample: FlameSample) -> None:
        """Synchronous on purpose, so a serial thread can call_soon_threadsafe it."""
        self._account_for_gap(sample.seq)
        self._latest = sample
        self._last_monotonic = time.monotonic()
        self._stats.state = "streaming"
        self._fanout(sample)

    def subscribe(self) -> asyncio.Queue[FlameSample]:
        queue: asyncio.Queue[FlameSample] = asyncio.Queue(maxsize=1)
        self._subscribers.add(queue)
        return queue

    def unsubscribe(self, queue: asyncio.Queue[FlameSample]) -> None:
        self._subscribers.discard(queue)

    def _fanout(self, sample: FlameSample) -> None:
        # Iterate a snapshot: a client may unsubscribe from another task.
        for queue in tuple(self._subscribers):
            if queue.full():
                with suppress(asyncio.QueueEmpty):
                    queue.get_nowait()
            with suppress(asyncio.QueueFull):
                queue.put_nowait(sample)

    def _account_for_gap(self, seq: int) -> None:
        if self._prev_seq is not None:
            gap = (seq - self._prev_seq - 1) % SEQ_MODULUS
            if 0 < gap < _MAX_PLAUSIBLE_GAP:
                self._stats.dropped_frames += gap
        self._prev_seq = seq

    def _set_state(self, state: LinkState) -> None:
        self._stats.state = state

    def _note_parse_error(self) -> None:
        self._stats.parse_errors += 1

    # ---- watchdog --------------------------------------------------------------

    async def _run_watchdog(self) -> None:
        """One task doing triple duty.

        It makes staleness visible to the dashboard, keeps the frame shape invariant when
        the source dies, and guarantees a send is *attempted* at least once a second so
        dead WebSocket clients are reaped promptly (a send-only endpoint never sees
        WebSocketDisconnect, which is raised by receive).
        """
        while True:
            await asyncio.sleep(_WATCHDOG_TICK_S)
            if self._latest is None:
                continue
            if time.monotonic() - self._last_monotonic > self._stale_after_s:
                self._stats.state = "disconnected"
                self._fanout(self._latest)

    # ---- read side -------------------------------------------------------------

    @property
    def latest(self) -> FlameSample | None:
        return self._latest

    @property
    def stats(self) -> LinkStats:
        """A snapshot with last_frame_age_ms computed at read time."""
        age_ms = int(max(time.monotonic() - self._last_monotonic, 0.0) * 1000)
        return LinkStats(
            source=self._stats.source,
            state=self._stats.state,
            dropped_frames=self._stats.dropped_frames,
            parse_errors=self._stats.parse_errors,
            last_frame_age_ms=age_ms,
        )

    @property
    def settings(self) -> Settings | None:
        return self._settings

    @property
    def subscriber_count(self) -> int:
        return len(self._subscribers)


telemetry_hub = TelemetryHub()
