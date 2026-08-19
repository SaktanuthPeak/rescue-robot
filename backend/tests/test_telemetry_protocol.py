"""FB1 line protocol decoding. Pure functions -- no app, no DB, no event loop."""

import pytest

from apiapp.infrastructure.flame_serial import SEQ_MODULUS, parse_line


@pytest.fixture(scope="function", autouse=True)
async def clean_db():
    """Shadow the autouse conftest fixture, which requires a running mongod."""
    yield


def _checksum(payload: str) -> str:
    check = 0
    for byte in payload.encode("ascii"):
        check ^= byte
    return f"{check:02X}"


def _line(payload: str) -> bytes:
    return f"{payload}*{_checksum(payload)}\r\n".encode("ascii")


class TestValidLines:
    def test_parses_a_well_formed_line(self):
        sample = parse_line(_line("FB1,812,118,1010,990,OK,4137"))
        assert sample is not None
        assert (sample.front, sample.right, sample.rear, sample.left) == (
            812,
            118,
            1010,
            990,
        )
        assert sample.status == "OK"
        assert sample.seq == 4137

    def test_accepts_a_line_with_no_checksum(self):
        """Hand-typed bring-up lines must work."""
        assert parse_line(b"FB1,0,0,0,0,OK,1\n") is not None

    def test_tolerates_crlf_termination(self):
        assert parse_line(b"FB1,1,2,3,4,OK,5\r\n") is not None

    @pytest.mark.parametrize("status", ["OK", "WARN", "FAULT"])
    def test_accepts_every_device_status(self, status):
        sample = parse_line(_line(f"FB1,1,2,3,4,{status},9"))
        assert sample is not None
        assert sample.status == status

    def test_accepts_the_rails(self):
        sample = parse_line(_line("FB1,0,1023,0,1023,OK,0"))
        assert sample is not None
        assert sample.front == 0
        assert sample.right == 1023


class TestRejectedLines:
    @pytest.mark.parametrize(
        "raw,reason",
        [
            (b"", "empty"),
            (b"\r\n", "blank"),
            (b"\xff\xfe\x00garbage\r\n", "bootloader binary noise"),
            (b"Ready\r\n", "boot banner"),
            (b"812,118,1010,990,OK,4137\n", "missing FB1 magic"),
            (b"FB2,1,2,3,4,OK,5\n", "wrong protocol version"),
            (b"FB1,1,2,3,OK,5\n", "too few fields"),
            (b"FB1,1,2,3,4,5,OK,6\n", "too many fields"),
            (b"FB1,1,2,3,4,BOOM,5\n", "unknown status token"),
            (b"FB1,1,2,3,4,OK\n", "missing seq"),
            (b"FB1,-1,2,3,4,OK,5\n", "negative value"),
            (b"FB1,1.5,2,3,4,OK,5\n", "non-integer value"),
            (b"FB1,15000,2,3,4,OK,5\n", "value beyond 4 digits"),
        ],
    )
    def test_discards_noise(self, raw, reason):
        assert parse_line(raw) is None, f"should have rejected: {reason}"

    def test_rejects_a_mismatching_checksum(self):
        """A flipped digit inside an otherwise-valid line must not become a reading."""
        assert parse_line(b"FB1,812,118,1010,990,OK,4137*00\r\n") is None

    def test_rejects_a_non_hex_checksum(self):
        assert parse_line(b"FB1,1,2,3,4,OK,5*ZZ\r\n") is None

    def test_rejects_a_truncated_line(self):
        assert parse_line(b"FB1,812,118,10") is None

    def test_rejects_seq_at_or_beyond_the_modulus(self):
        assert parse_line(_line(f"FB1,1,2,3,4,OK,{SEQ_MODULUS}")) is None

    def test_a_corrupted_digit_is_caught_by_the_checksum(self):
        """The whole reason the protocol carries a checksum at all."""
        good = _line("FB1,800,100,900,900,OK,10")
        corrupted = good.replace(b"FB1,800", b"FB1,100")
        assert parse_line(good) is not None
        assert parse_line(corrupted) is None
