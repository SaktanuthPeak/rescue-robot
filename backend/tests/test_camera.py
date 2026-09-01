from apiapp.core.config import get_settings
from apiapp.infrastructure.camera_service import camera_service, generate_mock_frame
from apiapp.modules.camera.schemas import CameraControlRequest
from apiapp.modules.camera.use_case import CameraUseCase


def test_generate_mock_frame():
    frame = generate_mock_frame(320, 240, frame_count=1)
    assert isinstance(frame, bytes)
    assert len(frame) > 100
    assert frame.startswith(b"\xff\xd8")
    assert frame.endswith(b"\xff\xd9")


def test_camera_service_sync():
    frame = camera_service.get_snapshot()
    assert isinstance(frame, bytes)
    assert frame.startswith(b"\xff\xd8")
    assert frame.endswith(b"\xff\xd9")

    st = camera_service.get_status()
    assert st.active is True
    assert st.width >= 160
    assert st.height >= 120
    assert st.fps >= 1


def test_camera_status_and_control_api():
    settings = get_settings()
    use_case = CameraUseCase(camera_service, settings)

    # Test snapshot
    snapshot = use_case.get_snapshot()
    assert snapshot.startswith(b"\xff\xd8")

    # Test status
    status = use_case.get_status()
    assert status.active is True

    # Test control
    ctrl_res = use_case.control_camera(CameraControlRequest(active=False))
    assert ctrl_res.active is False

    # Restore
    ctrl_res2 = use_case.control_camera(CameraControlRequest(active=True))
    assert ctrl_res2.active is True

    # Test config
    cfg = use_case.get_config()
    assert cfg.default_source in ("auto", "v4l2", "picam", "mock")
