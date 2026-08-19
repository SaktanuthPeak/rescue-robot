"""WebSocket streaming for /v1/telemetry/ws.

These are SYNC tests on purpose. httpx.AsyncClient cannot speak WebSocket, so they use
starlette.testclient.TestClient (zero new dependencies -- Starlette ships it and httpx is
already a dev dep). TestClient runs its own event loop in a thread, so it cannot reuse
conftest's session-scoped async `app` fixture; each test builds its own app.
pytest-asyncio's `asyncio_mode = "auto"` leaves plain `def` tests alone.
"""

import pytest
from starlette.testclient import TestClient

from apiapp.core.config import get_settings
from apiapp.run import create_app

SIDES = {"front", "right", "rear", "left"}


@pytest.fixture(scope="function", autouse=True)
async def clean_db():
    """Shadow the autouse conftest fixture, which requires a running mongod."""
    yield


async def _noop_init_beanie(*_args, **_kwargs):
    return None


@pytest.fixture
def ws_client(monkeypatch):
    """A TestClient with a hermetic app: mock telemetry source, no MongoDB.

    apiapp/run.py imports init_beanie by name, so apiapp.run.init_beanie is the correct
    patch target. Settings are mutated on the cached object because get_settings() is
    lru_cached and read at import time -- os.environ changes would have no effect.
    """
    monkeypatch.setattr("apiapp.run.init_beanie", _noop_init_beanie)
    settings = get_settings()
    monkeypatch.setattr(settings, "DATABASE_URI", "", raising=False)
    monkeypatch.setattr(settings, "TELEMETRY_SOURCE", "mock", raising=False)
    monkeypatch.setattr(settings, "TELEMETRY_MOCK_INTERVAL_MS", 10, raising=False)
    monkeypatch.setattr(settings, "TELEMETRY_MIN_BROADCAST_INTERVAL_MS", 0, raising=False)

    # `with` is required: it runs the lifespan, and routers only mount during lifespan.
    with TestClient(create_app()) as client:
        yield client


def test_first_frame_arrives_before_the_source_ticks(ws_client):
    """The endpoint sends a snapshot on connect, so the dashboard paints immediately."""
    with ws_client.websocket_connect("/v1/telemetry/ws") as ws:
        frame = ws.receive_json()

    assert frame["type"] == "telemetry"
    assert frame["v"] == 1
    assert set(frame["flame"]) == SIDES
    assert frame["link"]["source"] == "mock"


def test_frames_keep_arriving(ws_client):
    with ws_client.websocket_connect("/v1/telemetry/ws") as ws:
        frames = [ws.receive_json() for _ in range(5)]

    assert len(frames) == 5
    assert all(f["type"] == "telemetry" for f in frames)
    # The mock advances seq every tick, so live frames must not all be the seeded one.
    assert len({f["seq"] for f in frames}) > 1


def test_ws_payload_matches_the_rest_snapshot_shape(ws_client):
    """One TelemetryFrame model serves both, so REST and WS can never drift."""
    rest = ws_client.get("/v1/telemetry").json()
    with ws_client.websocket_connect("/v1/telemetry/ws") as ws:
        streamed = ws.receive_json()

    assert rest.keys() == streamed.keys()
    assert rest["flame"].keys() == streamed["flame"].keys()
    assert rest["link"].keys() == streamed["link"].keys()
    for side in SIDES:
        assert rest["flame"][side].keys() == streamed["flame"][side].keys()


def test_intensity_is_normalised_and_bounded(ws_client):
    with ws_client.websocket_connect("/v1/telemetry/ws") as ws:
        frames = [ws.receive_json() for _ in range(10)]

    for frame in frames:
        for side in SIDES:
            channel = frame["flame"][side]
            assert 0.0 <= channel["intensity"] <= 1.0
            assert 0 <= channel["raw"] <= frame["adc_max"]


def test_multiple_clients_each_get_their_own_stream(ws_client):
    """Fan-out: one client must not starve another."""
    with ws_client.websocket_connect("/v1/telemetry/ws") as first:
        with ws_client.websocket_connect("/v1/telemetry/ws") as second:
            a = [first.receive_json() for _ in range(3)]
            b = [second.receive_json() for _ in range(3)]

    assert len(a) == 3
    assert len(b) == 3


def test_a_disconnect_does_not_leak_a_subscriber(ws_client):
    """Without the `finally: unsubscribe(...)`, every disconnect leaks a queue forever."""
    from apiapp.infrastructure.telemetry_hub import telemetry_hub

    baseline = telemetry_hub.subscriber_count

    for _ in range(3):
        with ws_client.websocket_connect("/v1/telemetry/ws") as ws:
            ws.receive_json()

    assert telemetry_hub.subscriber_count == baseline


def test_a_disconnected_client_does_not_stop_the_others(ws_client):
    with ws_client.websocket_connect("/v1/telemetry/ws") as survivor:
        survivor.receive_json()
        with ws_client.websocket_connect("/v1/telemetry/ws") as doomed:
            doomed.receive_json()
        # `doomed` is now closed; the survivor must still receive frames.
        assert survivor.receive_json()["type"] == "telemetry"
