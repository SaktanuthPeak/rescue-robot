"""RB2/RB3 receiver telemetry protocol tests."""

from apiapp.infrastructure.receiver_canbus import parse_line


def _line(payload: str) -> bytes:
    checksum = 0
    for byte in payload.encode("ascii"):
        checksum ^= byte
    return f"{payload}*{checksum:02X}\r\n".encode("ascii")


def test_parses_arm_controller_rb2_frame() -> None:
    sample = parse_line(_line("RB2,1,1,10,1,0,0,42"))

    assert sample is not None
    assert sample.motor_code == 1
    assert sample.motor_alive is True
    assert sample.arm_code == 10
    assert sample.arm_alive is True
    assert sample.battery_millivolts == 0
    assert sample.battery_adc == 0
    assert sample.flame_valid is False
    assert sample.sequence == 42


def test_rejects_receiver_frame_with_bad_checksum() -> None:
    assert parse_line(b"RB2,1,1,10,1,0,0,42*00\n") is None

