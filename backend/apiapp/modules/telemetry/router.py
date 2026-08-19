from fastapi import APIRouter, Depends, WebSocket

from .schemas import TelemetryConfig, TelemetryFrame
from .use_case import TelemetryUseCase, get_telemetry_use_case

router = APIRouter(prefix="/v1/telemetry", tags=["Telemetry"])


@router.get("", response_model=TelemetryFrame, summary="Latest Telemetry Snapshot")
async def get_telemetry_snapshot(
    use_case: TelemetryUseCase = Depends(get_telemetry_use_case),
) -> TelemetryFrame:
    """Latest flame reading.

    Always returns 200 -- absent hardware is reported in ``link.state``, not as an error.
    Useful for the dashboard's first paint and for OpenAPI type generation.
    """
    return use_case.get_snapshot()


@router.get("/config", response_model=TelemetryConfig, summary="Telemetry Calibration")
async def get_telemetry_config(
    use_case: TelemetryUseCase = Depends(get_telemetry_use_case),
) -> TelemetryConfig:
    """Polarity, threshold and ADC scale, so the dashboard need not hardcode them."""
    return use_case.get_config()


@router.websocket("/ws")
async def stream_telemetry(
    websocket: WebSocket,
    use_case: TelemetryUseCase = Depends(get_telemetry_use_case),
) -> None:
    """Stream TelemetryFrame JSON, identical in shape to the REST snapshot."""
    await use_case.stream(websocket)
