"""Telemetry DTOs. These field names are the frontend's contract -- see docs/firebot-spec.md.

``TelemetryFrame`` is deliberately used as both the REST snapshot's ``response_model``
*and* the exact WebSocket payload. WebSocket routes never appear in ``openapi.json``, so
declaring the shape on the REST endpoint is what lets the frontend see it via
``pnpm openapi`` -- and reusing one model is what stops REST and WS from drifting.
"""

from typing import Literal, Optional

from pydantic import BaseModel, Field

FlameDirection = Literal["front", "right", "rear", "left"]
LinkState = Literal["streaming", "connecting", "disconnected"]
SourceKind = Literal["mock", "serial"]
DeviceStatus = Literal["OK", "WARN", "FAULT"]

TELEMETRY_PROTOCOL_VERSION = 1


class FlameChannel(BaseModel):
    """One directional flame sensor."""

    raw: int = Field(
        ..., ge=0, description="Untouched ADC reading. Debug/plot use only."
    )
    intensity: float = Field(
        ...,
        ge=0.0,
        le=1.0,
        description=(
            "Polarity-normalised flame strength. 1.0 is the strongest flame. "
            "Drive all visuals from this, never from raw."
        ),
    )
    detected: bool = Field(
        ..., description="Whether this channel crossed the configured threshold."
    )


class FlameChannels(BaseModel):
    """All four sides. Never null, so the frontend's shape is invariant."""

    front: FlameChannel
    right: FlameChannel
    rear: FlameChannel
    left: FlameChannel


class TelemetryLink(BaseModel):
    """Health of the Arduino link, as seen by the backend."""

    source: SourceKind = Field(
        ..., description="Which transport produced this frame. 'mock' means simulated."
    )
    state: LinkState = Field(
        ...,
        description="Anything other than 'streaming' means the gauges are not live.",
    )
    last_frame_age_ms: int = Field(
        ...,
        ge=0,
        description=(
            "Milliseconds since the last real sample, measured server-side with a "
            "monotonic clock so it is immune to browser/server clock skew."
        ),
    )
    dropped_frames: int = Field(
        ..., ge=0, description="Cumulative frames inferred missing from seq gaps."
    )
    parse_errors: int = Field(
        ..., ge=0, description="Cumulative rejected lines (bad magic, checksum, range)."
    )


class TelemetryFrame(BaseModel):
    """One telemetry frame. The only message type on the WebSocket in Phase 1."""

    type: Literal["telemetry"] = Field(
        default="telemetry", description="Message discriminator."
    )
    v: int = Field(
        default=TELEMETRY_PROTOCOL_VERSION,
        description="Protocol version. A mismatch means the dashboard must be updated.",
    )
    ts: int = Field(
        ...,
        description="Epoch milliseconds when the backend received the sample.",
    )
    seq: int = Field(..., ge=0, description="Device frame counter, wraps at 65536.")
    status: DeviceStatus = Field(
        ..., description="Device-reported wiring health, orthogonal to flame detection."
    )
    adc_max: int = Field(
        ..., gt=0, description="ADC full scale, so the UI need not hardcode 1023."
    )
    link: TelemetryLink
    flame: FlameChannels
    flame_detected: bool = Field(
        ..., description="True if any channel crossed the threshold."
    )
    strongest_direction: Optional[FlameDirection] = Field(
        default=None,
        description=(
            "Highest-intensity channel among those detected, else null. "
            "Ties resolve front, right, rear, left."
        ),
    )


class TelemetryConfig(BaseModel):
    """Backend calibration, so the dashboard can draw a threshold without guessing."""

    source: SourceKind
    adc_max: int = Field(..., gt=0)
    threshold: int = Field(..., ge=0, description="Raw ADC threshold for 'detected'.")
    active_low: bool = Field(
        ...,
        description="True when a lower ADC reading means more flame (typical YG1006).",
    )
    threshold_intensity: float = Field(
        ...,
        ge=0.0,
        le=1.0,
        description="The threshold expressed on the same 0..1 scale as intensity.",
    )
    mock_interval_ms: int = Field(..., gt=0)
    stale_after_ms: int = Field(..., gt=0)
