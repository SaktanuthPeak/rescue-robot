from collections.abc import AsyncGenerator

from ...core.config import Settings, get_settings
from ...infrastructure.camera_service import CameraService, camera_service
from .schemas import CameraConfigResponse, CameraControlRequest, CameraStatusResponse


class CameraUseCase:
    def __init__(self, service: CameraService, settings: Settings) -> None:
        self._service = service
        self._settings = settings

    def get_snapshot(self) -> bytes:
        return self._service.get_snapshot()

    def get_stream(self) -> AsyncGenerator[bytes, None]:
        return self._service.mjpeg_stream()

    def get_status(self) -> CameraStatusResponse:
        st = self._service.get_status()
        return CameraStatusResponse(
            active=st.active,
            source=st.source,
            device=st.device,
            width=st.width,
            height=st.height,
            fps=st.fps,
            frame_count=st.frame_count,
            last_frame_age_ms=st.last_frame_age_ms,
            is_hardware=st.is_hardware,
        )

    def control_camera(self, req: CameraControlRequest) -> CameraStatusResponse:
        if req.active is not None:
            self._service.set_active(req.active)
        if req.width is not None or req.height is not None or req.fps is not None:
            self._service.set_config(width=req.width, height=req.height, fps=req.fps)
        return self.get_status()

    def get_config(self) -> CameraConfigResponse:
        st = self._service.get_status()
        return CameraConfigResponse(
            default_device=self._settings.CAMERA_DEVICE,
            default_source=self._settings.CAMERA_SOURCE,
            width=st.width,
            height=st.height,
            fps=st.fps,
            auto_start=self._settings.CAMERA_AUTO_START,
        )


def get_camera_use_case() -> CameraUseCase:
    return CameraUseCase(camera_service, get_settings())
