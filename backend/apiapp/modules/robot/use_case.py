from fastapi import HTTPException, status

from ...core.config import get_settings
from ...infrastructure.receiver_canbus import receiver_canbus_service
from .schemas import RobotCommandRequest, RobotCommandResponse, RobotStatusResponse


class RobotUseCase:
    def get_status(self) -> RobotStatusResponse:
        return RobotStatusResponse.model_validate(receiver_canbus_service.status())

    def send_command(self, request: RobotCommandRequest) -> RobotCommandResponse:
        if request.channel == "motor" and request.code > 8:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Motor codes must be between 0 and 8",
            )
        if request.channel == "all" and request.code != 0:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="The all channel only supports STOP (code 0)",
            )

        command = f"CMD:{request.channel.upper()}:{request.code}"
        if not receiver_canbus_service.send_command(request.channel, request.code):
            raise HTTPException(
                status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                detail="Receiver Arduino USB serial is not connected",
            )

        return RobotCommandResponse(
            accepted=True,
            command=command,
            message="Command sent to receiver Arduino",
        )


def get_robot_use_case() -> RobotUseCase:
    # Keep the provider shape consistent with the other feature modules.
    get_settings()
    return RobotUseCase()
