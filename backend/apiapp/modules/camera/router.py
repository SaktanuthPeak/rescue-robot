from fastapi import APIRouter, Depends, Response
from fastapi.responses import StreamingResponse

from .schemas import CameraConfigResponse, CameraControlRequest, CameraStatusResponse
from .use_case import CameraUseCase, get_camera_use_case

router = APIRouter(prefix="/v1/camera", tags=["Camera"])


@router.get(
    "/stream",
    summary="Live Camera MJPEG Stream",
    response_class=StreamingResponse,
    responses={
        200: {
            "content": {"multipart/x-mixed-replace; boundary=frame": {}},
            "description": "Continuous MJPEG stream from Raspberry Pi / Mock camera.",
        }
    },
)
async def stream_camera(
    use_case: CameraUseCase = Depends(get_camera_use_case),
) -> StreamingResponse:
    """Stream live camera frames in standard multipart/x-mixed-replace MJPEG format.

    Directly viewable in HTML: ``<img src="/v1/camera/stream" />``.
    """
    return StreamingResponse(
        use_case.get_stream(),
        media_type="multipart/x-mixed-replace; boundary=frame",
        headers={
            "Cache-Control": "no-cache, no-store, must-revalidate, pre-check=0, post-check=0, max-age=0",
            "Pragma": "no-cache",
            "Expires": "0",
            "Connection": "close",
        },
    )


@router.get(
    "/snapshot",
    summary="Get Single Camera Snapshot",
    response_class=Response,
    responses={
        200: {
            "content": {"image/jpeg": {}},
            "description": "Latest single captured frame as JPEG.",
        }
    },
)
async def get_camera_snapshot(
    use_case: CameraUseCase = Depends(get_camera_use_case),
) -> Response:
    """Returns the latest captured frame as a single JPEG image."""
    frame_bytes = use_case.get_snapshot()
    return Response(
        content=frame_bytes,
        media_type="image/jpeg",
        headers={
            "Cache-Control": "no-cache, no-store, must-revalidate",
            "Content-Length": str(len(frame_bytes)),
        },
    )


@router.get(
    "/status",
    response_model=CameraStatusResponse,
    summary="Get Camera Status",
)
async def get_camera_status(
    use_case: CameraUseCase = Depends(get_camera_use_case),
) -> CameraStatusResponse:
    """Get active streaming status, resolution, FPS, and source mode."""
    return use_case.get_status()


@router.post(
    "/control",
    response_model=CameraStatusResponse,
    summary="Control Camera Settings",
)
async def control_camera(
    req: CameraControlRequest,
    use_case: CameraUseCase = Depends(get_camera_use_case),
) -> CameraStatusResponse:
    """Turn video stream on/off or change resolution/framerate."""
    return use_case.control_camera(req)


@router.get(
    "/config",
    response_model=CameraConfigResponse,
    summary="Get Camera Configuration",
)
async def get_camera_config(
    use_case: CameraUseCase = Depends(get_camera_use_case),
) -> CameraConfigResponse:
    """Get default camera configuration settings."""
    return use_case.get_config()
