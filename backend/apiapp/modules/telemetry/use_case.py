"""Telemetry business logic: polarity/threshold interpretation and the WebSocket stream.

Protocol *decoding* is a transport concern and lives in
``apiapp/infrastructure/flame_serial.py``. Deciding what a raw ADC number *means* is
business logic and lives here.
"""

from __future__ import annotations

import asyncio
from contextlib import suppress
from datetime import UTC, datetime

from fastapi import WebSocket, WebSocketDisconnect
from loguru import logger

from ...core.config import Settings, get_settings
from ...infrastructure.flame_serial import FlameSample
from ...infrastructure.telemetry_hub import telemetry_hub
from .schemas import (
    FlameChannel,
    FlameChannels,
    FlameDirection,
    TelemetryConfig,
    TelemetryFrame,
    TelemetryLink,
)

# Frontend reads these in the order the wire declares them; ties resolve front-first.
_DIRECTIONS: tuple[FlameDirection, ...] = ("front", "right", "rear", "left")


class TelemetryUseCase:
    """Stateless per request; reads through to the process-wide hub."""

    def __init__(self) -> None:
        self._hub = telemetry_hub

    # ---- mapping ---------------------------------------------------------------

    def _settings(self) -> Settings:
        # Prefer the settings the hub was started with, so a mid-run settings change
        # cannot make REST and WS disagree about polarity.
        return self._hub.settings or get_settings()

    @staticmethod
    def _intensity(raw: int, adc_max: int, active_low: bool) -> float:
        if adc_max <= 0:
            return 0.0
        ratio = (adc_max - raw) / adc_max if active_low else raw / adc_max
        return round(min(max(ratio, 0.0), 1.0), 3)

    @staticmethod
    def _detected(raw: int, threshold: int, active_low: bool) -> bool:
        return raw <= threshold if active_low else raw >= threshold

    def _channel(self, raw: int, settings: Settings) -> FlameChannel:
        return FlameChannel(
            raw=raw,
            intensity=self._intensity(
                raw, settings.FLAME_ADC_MAX, settings.FLAME_ACTIVE_LOW
            ),
            detected=self._detected(
                raw, settings.FLAME_THRESHOLD, settings.FLAME_ACTIVE_LOW
            ),
        )

    def _to_frame(self, sample: FlameSample) -> TelemetryFrame:
        """The single sample -> frame mapping, used by BOTH REST and WS so they can't drift."""
        settings = self._settings()
        channels = {
            "front": self._channel(sample.front, settings),
            "right": self._channel(sample.right, settings),
            "rear": self._channel(sample.rear, settings),
            "left": self._channel(sample.left, settings),
        }
        stats = self._hub.stats

        detected = [d for d in _DIRECTIONS if channels[d].detected]
        strongest: FlameDirection | None = None
        if detected:
            # max() keeps the first maximum, and _DIRECTIONS is the tie-break order.
            strongest = max(detected, key=lambda d: channels[d].intensity)

        return TelemetryFrame(
            ts=int(sample.received_at.timestamp() * 1000),
            seq=sample.seq,
            status=sample.status,
            adc_max=settings.FLAME_ADC_MAX,
            link=TelemetryLink(
                source=stats.source,
                state=stats.state,
                last_frame_age_ms=stats.last_frame_age_ms,
                dropped_frames=stats.dropped_frames,
                parse_errors=stats.parse_errors,
            ),
            flame=FlameChannels(**channels),
            flame_detected=bool(detected),
            strongest_direction=strongest,
        )

    # ---- read endpoints --------------------------------------------------------

    def get_snapshot(self) -> TelemetryFrame:
        """Always succeeds. Absent hardware is a state, not an error."""
        settings = self._settings()
        sample = self._hub.latest
        if sample is None:
            # Hub not started (or already stopped): synthesise the idle rail so the
            # frame shape stays invariant instead of 404-ing.
            rail = settings.FLAME_ADC_MAX if settings.FLAME_ACTIVE_LOW else 0
            sample = FlameSample(
                front=rail,
                right=rail,
                rear=rail,
                left=rail,
                status="FAULT",
                seq=0,
                received_at=datetime.now(UTC),
            )
        return self._to_frame(sample)

    def get_config(self) -> TelemetryConfig:
        settings = self._settings()
        return TelemetryConfig(
            source=settings.TELEMETRY_SOURCE,
            adc_max=settings.FLAME_ADC_MAX,
            threshold=settings.FLAME_THRESHOLD,
            active_low=settings.FLAME_ACTIVE_LOW,
            threshold_intensity=self._intensity(
                settings.FLAME_THRESHOLD,
                settings.FLAME_ADC_MAX,
                settings.FLAME_ACTIVE_LOW,
            ),
            mock_interval_ms=settings.TELEMETRY_MOCK_INTERVAL_MS,
            stale_after_ms=settings.TELEMETRY_STALE_AFTER_MS,
        )

    # ---- stream ----------------------------------------------------------------

    async def stream(self, websocket: WebSocket) -> None:
        """Send-only telemetry stream.

        Note the exception set: a send-only endpoint never sees ``WebSocketDisconnect``
        (that is raised by ``receive``). A dead peer surfaces as ``RuntimeError`` from
        ``send`` or a ``ConnectionError``, so all three must be caught. The hub's 1 Hz
        watchdog guarantees a send is attempted every second, which is what makes dead
        clients get reaped promptly.
        """
        settings = self._settings()
        floor_s = max(settings.TELEMETRY_MIN_BROADCAST_INTERVAL_MS, 0) / 1000.0

        await websocket.accept()
        queue = self._hub.subscribe()
        try:
            # Paint immediately, before waiting on the source to tick.
            await websocket.send_text(self.get_snapshot().model_dump_json())
            while True:
                sample = await queue.get()
                await websocket.send_text(self._to_frame(sample).model_dump_json())
                if floor_s:
                    await asyncio.sleep(floor_s)
        except (WebSocketDisconnect, RuntimeError, ConnectionError) as exc:
            logger.debug(f"telemetry websocket closed: {exc!r}")
        except asyncio.CancelledError:
            raise
        finally:
            # Mandatory: without this every disconnect leaks a queue that the hub's
            # fan-out keeps filling forever.
            self._hub.unsubscribe(queue)
            with suppress(Exception):
                await websocket.close()


def get_telemetry_use_case() -> TelemetryUseCase:
    """Get TelemetryUseCase instance"""
    return TelemetryUseCase()
