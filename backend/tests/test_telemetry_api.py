"""REST contract for /v1/telemetry.

These assertions are the frontend's contract: the exact field names and nesting the
dashboard's Zod schema parses. If this file needs editing, the frontend needs updating
too -- see docs/firebot-spec.md.

It also doubles as the guard against a silent router-import failure: core/router.py has a
bare `except ImportError: continue`, so a broken telemetry module makes the endpoint
vanish with no traceback, and nothing but this test would notice.
"""

import pytest

from apiapp.modules.telemetry.schemas import TELEMETRY_PROTOCOL_VERSION

SIDES = {"front", "right", "rear", "left"}


@pytest.fixture(scope="function", autouse=True)
async def clean_db():
    """Shadow the autouse conftest fixture, which requires a running mongod."""
    yield


class TestSnapshotEndpoint:
    async def test_returns_200_even_with_no_hardware_attached(self, client):
        """Absent hardware is a state, not an error -- there is no 404/503 path."""
        response = await client.get("/v1/telemetry")
        assert response.status_code == 200

    async def test_envelope_fields(self, client):
        body = (await client.get("/v1/telemetry")).json()
        assert body["type"] == "telemetry"
        assert body["v"] == TELEMETRY_PROTOCOL_VERSION
        assert isinstance(body["ts"], int)
        assert body["status"] in {"OK", "WARN", "FAULT"}
        assert body["adc_max"] > 0
        assert isinstance(body["seq"], int)

    async def test_all_four_sides_are_always_present(self, client):
        """flame is never null and never partial, so the UI's shape is invariant."""
        body = (await client.get("/v1/telemetry")).json()
        assert set(body["flame"]) == SIDES

    async def test_every_channel_has_raw_intensity_and_detected(self, client):
        body = (await client.get("/v1/telemetry")).json()
        for side in SIDES:
            channel = body["flame"][side]
            assert set(channel) == {"raw", "intensity", "detected"}
            assert isinstance(channel["raw"], int)
            assert 0.0 <= channel["intensity"] <= 1.0
            assert isinstance(channel["detected"], bool)

    async def test_link_block_shape(self, client):
        body = (await client.get("/v1/telemetry")).json()
        link = body["link"]
        assert set(link) == {
            "source",
            "state",
            "last_frame_age_ms",
            "dropped_frames",
            "parse_errors",
        }
        assert link["source"] in {"mock", "serial"}
        assert link["state"] in {"streaming", "connecting", "disconnected"}
        assert link["last_frame_age_ms"] >= 0

    async def test_flame_detected_agrees_with_the_channels(self, client):
        body = (await client.get("/v1/telemetry")).json()
        expected = any(body["flame"][side]["detected"] for side in SIDES)
        assert body["flame_detected"] is expected

    async def test_strongest_direction_is_a_detected_side_or_null(self, client):
        body = (await client.get("/v1/telemetry")).json()
        strongest = body["strongest_direction"]
        if strongest is None:
            assert not body["flame_detected"]
        else:
            assert strongest in SIDES
            assert body["flame"][strongest]["detected"] is True

    async def test_trailing_slash_is_not_the_canonical_path(self, client):
        """Router uses "" not "/", so /v1/telemetry/ redirects. Frontend must omit it."""
        response = await client.get("/v1/telemetry/")
        assert response.status_code in (307, 404)


class TestConfigEndpoint:
    async def test_returns_calibration(self, client):
        response = await client.get("/v1/telemetry/config")
        assert response.status_code == 200
        body = response.json()
        assert set(body) == {
            "source",
            "adc_max",
            "threshold",
            "active_low",
            "threshold_intensity",
            "mock_interval_ms",
            "stale_after_ms",
        }

    async def test_threshold_intensity_is_on_the_same_scale_as_channels(self, client):
        body = (await client.get("/v1/telemetry/config")).json()
        assert 0.0 <= body["threshold_intensity"] <= 1.0

    async def test_detected_flag_agrees_with_the_published_threshold(self, client):
        """The UI draws a threshold line from /config; it must match reality."""
        config = (await client.get("/v1/telemetry/config")).json()
        frame = (await client.get("/v1/telemetry")).json()
        for side in SIDES:
            channel = frame["flame"][side]
            expected = (
                channel["raw"] <= config["threshold"]
                if config["active_low"]
                else channel["raw"] >= config["threshold"]
            )
            assert channel["detected"] is expected, f"{side} disagrees with threshold"


class TestPolarityMapping:
    """Unit-level checks on the raw -> intensity mapping, independent of live data."""

    def test_active_low_inverts(self):
        from apiapp.modules.telemetry.use_case import TelemetryUseCase

        intensity = TelemetryUseCase._intensity
        assert intensity(0, 1023, active_low=True) == 1.0
        assert intensity(1023, 1023, active_low=True) == 0.0

    def test_active_high_does_not_invert(self):
        from apiapp.modules.telemetry.use_case import TelemetryUseCase

        intensity = TelemetryUseCase._intensity
        assert intensity(0, 1023, active_low=False) == 0.0
        assert intensity(1023, 1023, active_low=False) == 1.0

    def test_out_of_range_raw_is_clamped_not_wrapped(self):
        from apiapp.modules.telemetry.use_case import TelemetryUseCase

        intensity = TelemetryUseCase._intensity
        assert intensity(5000, 1023, active_low=True) == 0.0
        assert intensity(5000, 1023, active_low=False) == 1.0

    def test_zero_scale_does_not_divide_by_zero(self):
        from apiapp.modules.telemetry.use_case import TelemetryUseCase

        assert TelemetryUseCase._intensity(10, 0, active_low=True) == 0.0
