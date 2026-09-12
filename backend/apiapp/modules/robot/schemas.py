from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field

RobotChannel = Literal["motor", "arm", "all"]
RobotLinkState = Literal["disabled", "connecting", "streaming", "disconnected"]


class RobotStatusResponse(BaseModel):
    type: Literal["robot_status"]
    protocol: Literal["RB1", "RB2", "RB3", "RB4"] | None
    port: str
    baudrate: int
    state: RobotLinkState
    last_frame_age_ms: int = Field(ge=0)
    parse_errors: int = Field(ge=0)
    sequence: int = Field(ge=0)
    motor_code: int
    motor_status: str
    motor_can_alive: bool
    arm_code: int
    arm_status: str
    arm_can_alive: bool
    arm_axis_1_pwm: int | None
    arm_axis_2_pwm: int | None
    arm_axis_3_pwm: int | None
    arm_pump_on: bool | None
    battery_millivolts: int = Field(ge=0)
    battery_volts: float = Field(ge=0)
    battery_adc: int = Field(ge=0)
    last_command: str | None
    last_command_at: datetime | None


class RobotCommandRequest(BaseModel):
    channel: RobotChannel
    code: int = Field(ge=0, le=14)


class RobotCommandResponse(BaseModel):
    accepted: bool
    command: str
    message: str
