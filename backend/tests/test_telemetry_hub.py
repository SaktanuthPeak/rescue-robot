"""TelemetryHub fan-out, seq accounting, and lifecycle. Pure asyncio -- no app, no DB."""

import asyncio
from datetime import UTC, datetime

import pytest

from apiapp.core.config import get_settings
from apiapp.infrastructure.flame_serial import SEQ_MODULUS, FlameSample
from apiapp.infrastructure.telemetry_hub import TelemetryHub


@pytest.fixture(scope="function", autouse=True)
async def clean_db():
    """Shadow the autouse conftest fixture, which requires a running mongod."""
    yield


def make_sample(seq: int = 1, front: int = 500) -> FlameSample:
    return FlameSample(
        front=front,
        right=900,
        rear=900,
        left=900,
        status="OK",
        seq=seq,
        received_at=datetime.now(UTC),
    )


@pytest.fixture
def hub() -> TelemetryHub:
    return TelemetryHub()


class TestFanout:
    async def test_publish_reaches_every_subscriber(self, hub):
        a, b = hub.subscribe(), hub.subscribe()
        sample = make_sample()

        hub.publish(sample)

        assert a.get_nowait() is sample
        assert b.get_nowait() is sample

    async def test_a_full_queue_coalesces_to_the_newest_sample(self, hub):
        """Latest-wins: a slow client sees the newest frame, never a stale backlog."""
        queue = hub.subscribe()
        first, second, third = make_sample(1), make_sample(2), make_sample(3)

        hub.publish(first)
        hub.publish(second)
        hub.publish(third)

        assert queue.qsize() == 1
        assert queue.get_nowait() is third

    async def test_unsubscribe_stops_delivery(self, hub):
        queue = hub.subscribe()
        hub.unsubscribe(queue)

        hub.publish(make_sample())

        assert queue.empty()

    async def test_unsubscribe_is_idempotent(self, hub):
        queue = hub.subscribe()
        hub.unsubscribe(queue)
        hub.unsubscribe(queue)  # must not raise
        assert hub.subscriber_count == 0

    async def test_one_stalled_client_does_not_block_the_others(self, hub):
        """The whole point of a maxsize=1 latest-wins queue per client."""
        stalled, healthy = hub.subscribe(), hub.subscribe()
        hub.publish(make_sample(1))  # fills both; nobody drains `stalled`

        hub.publish(make_sample(2))
        healthy.get_nowait()
        hub.publish(make_sample(3))

        assert healthy.get_nowait().seq == 3
        assert stalled.qsize() == 1


class TestSeqAccounting:
    async def test_consecutive_frames_drop_nothing(self, hub):
        for seq in range(1, 6):
            hub.publish(make_sample(seq))
        assert hub.stats.dropped_frames == 0

    async def test_a_gap_of_three_counts_three(self, hub):
        hub.publish(make_sample(10))
        hub.publish(make_sample(14))
        assert hub.stats.dropped_frames == 3

    async def test_uint16_wrap_is_not_a_gap(self, hub):
        hub.publish(make_sample(SEQ_MODULUS - 1))
        hub.publish(make_sample(0))
        assert hub.stats.dropped_frames == 0

    async def test_a_board_reset_is_not_counted_as_dropped_frames(self, hub):
        """An implausible jump means the device restarted, not that 40k frames vanished."""
        hub.publish(make_sample(50_000))
        hub.publish(make_sample(1))
        assert hub.stats.dropped_frames == 0

    async def test_the_first_frame_never_reports_a_gap(self, hub):
        hub.publish(make_sample(9999))
        assert hub.stats.dropped_frames == 0


class TestLifecycle:
    async def test_start_is_idempotent(self):
        """create_app() installs lifespan twice and tests build several apps per process."""
        hub = TelemetryHub()
        settings = get_settings()
        try:
            await hub.start(settings)
            first_latest = hub.latest
            await hub.start(settings)
            assert first_latest is hub.latest, "second start() rebuilt the source"
        finally:
            await hub.stop()

    async def test_stop_then_start_works_again(self):
        hub = TelemetryHub()
        settings = get_settings()
        await hub.start(settings)
        await hub.stop()
        assert hub.latest is None

        await hub.start(settings)
        try:
            assert hub.latest is not None
        finally:
            await hub.stop()

    async def test_stop_is_safe_without_start(self):
        await TelemetryHub().stop()  # must not raise

    def test_start_rebinds_when_called_from_a_different_event_loop(self):
        """Regression: idempotence must be scoped to the current loop, not global.

        The hub is a process-wide singleton holding asyncio primitives owned by whichever
        loop created them. A plain `if self._source is not None: return` made a second
        loop's start() a no-op, so its WebSocket handlers awaited queues that were only
        ever fed from the first (now idle) loop -- hanging forever instead of streaming.
        """
        hub = TelemetryHub()
        settings = get_settings()

        async def start_and_capture():
            await hub.start(settings)
            return asyncio.get_running_loop(), hub.subscribe()

        first_loop = asyncio.new_event_loop()
        try:
            loop_a, queue_a = first_loop.run_until_complete(start_and_capture())
        finally:
            first_loop.close()

        second_loop = asyncio.new_event_loop()
        try:
            loop_b, queue_b = second_loop.run_until_complete(start_and_capture())
            assert loop_a is not loop_b
            # Rebound: the new queue belongs to the live loop, and the stale one is gone.
            assert queue_b is not queue_a
            assert queue_a not in hub._subscribers
            assert queue_b in hub._subscribers
            second_loop.run_until_complete(hub.stop())
        finally:
            second_loop.close()

    async def test_start_seeds_an_idle_sample_at_the_no_flame_rail(self):
        """Guarantees the frame shape is invariant from the very first request."""
        hub = TelemetryHub()
        settings = get_settings()
        try:
            await hub.start(settings)
            sample = hub.latest
            assert sample is not None
            rail = (
                settings.FLAME_ADC_MAX if settings.FLAME_ACTIVE_LOW else 0
            )
            assert sample.front == rail
            assert sample.status == "FAULT"
            assert sample.seq == 0
        finally:
            await hub.stop()

    async def test_the_mock_source_actually_publishes(self):
        hub = TelemetryHub()
        settings = get_settings()
        try:
            await hub.start(settings)
            queue = hub.subscribe()
            sample = await asyncio.wait_for(queue.get(), timeout=3.0)
            assert 0 <= sample.front <= settings.FLAME_ADC_MAX
            assert hub.stats.state == "streaming"
        finally:
            await hub.stop()


class TestStats:
    async def test_parse_errors_accumulate(self, hub):
        hub._note_parse_error()
        hub._note_parse_error()
        assert hub.stats.parse_errors == 2

    async def test_last_frame_age_is_measured_not_stored(self, hub):
        hub.publish(make_sample())
        assert hub.stats.last_frame_age_ms >= 0
        await asyncio.sleep(0.05)
        assert hub.stats.last_frame_age_ms >= 40
