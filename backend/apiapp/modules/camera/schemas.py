from typing import Literal

from pydantic import BaseModel, Field


class CameraStatusResponse(BaseModel):
    active: bool = Field(description="Whether video streaming / capture is active")
    source: Literal["auto", "v4l2", "picam", "mock"] = Field(description="Active video source mode")
    device: str = Field(description="Configured video device path, e.g. /dev/video0")
    width: int = Field(description="Frame width in pixels")
    height: int = Field(description="Frame height in pixels")
    fps: int = Field(description="Frame rate target")
    frame_count: int = Field(description="Total frames generated/captured since start")
    last_frame_age_ms: int = Field(description="Milliseconds since the last frame was captured")
    is_hardware: bool = Field(description="True if captured from physical camera hardware, False if mock")


class CameraControlRequest(BaseModel):
    active: bool | None = Field(default=None, description="Turn camera capture on (true) or off (false)")
    width: int | None = Field(default=None, ge=160, le=1280, description="Set resolution width")
    height: int | None = Field(default=None, ge=120, le=720, description="Set resolution height")
    fps: int | None = Field(default=None, ge=1, le=30, description="Set target frames per second")


class CameraConfigResponse(BaseModel):
    default_device: str
    default_source: str
    width: int
    height: int
    fps: int
    auto_start: bool
