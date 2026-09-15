"""RB2/RB3 receiver telemetry protocol tests."""

from apiapp.infrastructure.receiver_canbus import STATUS_NAMES, parse_line


def _line(payload: str) -> bytes:
    checksum = 0
    for byte in payload.encode("ascii"):
        checksum ^= byte
    return f"{payload}*{checksum:02X}\r\n".encode("ascii")


def test_parses_arm_controller_rb2_frame() -> None:
    sample = parse_line(_line("RB2,1,1,14,1,0,0,42"))

    assert sample is not None
    assert sample.protocol == "RB2"
    assert sample.motor_code == 1
    assert sample.motor_alive is True
    assert sample.arm_code == 14
    assert STATUS_NAMES[sample.arm_code] == "HEAD_DOWN"
    assert sample.arm_alive is True
    assert sample.battery_millivolts == 0
    assert sample.battery_adc == 0
    assert sample.flame_valid is False
    assert sample.arm_axis_1_pwm is None
    assert sample.arm_axis_2_pwm is None
    assert sample.arm_axis_3_pwm is None
    assert sample.arm_pump_on is None
    assert sample.sequence == 42


def test_rejects_receiver_frame_with_bad_checksum() -> None:
    assert parse_line(b"RB2,1,1,10,1,0,0,42*00\n") is None


def test_parses_receiver_rb3_frame_with_ir_and_dht11_humidity() -> None:
    sample = parse_line(_line("RB3,-1,0,-1,0,7400,303,100,200,300,400,65,44"))

    assert sample is not None
    assert sample.protocol == "RB3"
    assert sample.battery_millivolts == 7400
    assert sample.battery_adc == 303
    assert (sample.flame_front, sample.flame_right, sample.flame_rear, sample.flame_left) == (
        100,
        200,
        300,
        400,
    )
    assert sample.flame_valid is True
    assert sample.humidity_percent == 65.0
    assert sample.sequence == 44


def test_parses_legacy_receiver_rb3_frame_without_humidity() -> None:
    sample = parse_line(_line("RB3,-1,0,-1,0,7400,303,100,200,300,400,43"))

    assert sample is not None
    assert sample.humidity_percent is None
    assert sample.sequence == 43


def test_parses_arm_controller_rb4_pose_frame() -> None:
    sample = parse_line(_line("RB4,6,1,13,1,0,0,305,278,331,1,43"))

    assert sample is not None
    assert sample.protocol == "RB4"
    assert sample.motor_code == 6
    assert sample.motor_alive is True
    assert sample.arm_code == 13
    assert sample.arm_alive is True
    assert sample.arm_axis_1_pwm == 305
    assert sample.arm_axis_2_pwm == 278
    assert sample.arm_axis_3_pwm == 331
    assert sample.arm_pump_on is True
    assert sample.sequence == 43


def test_rejects_arm_controller_rb4_invalid_pose() -> None:
    assert parse_line(_line("RB4,6,1,13,1,0,0,4096,278,331,0,43")) is None
    assert parse_line(_line("RB4,6,1,13,1,0,0,305,278,331,2,43")) is None
