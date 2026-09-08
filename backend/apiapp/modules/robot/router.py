from fastapi import APIRouter, Depends

from .schemas import RobotCommandRequest, RobotCommandResponse, RobotStatusResponse
from .use_case import RobotUseCase, get_robot_use_case

router = APIRouter(prefix="/v1/robot", tags=["Robot Control"])


@router.get("/status", response_model=RobotStatusResponse)
async def get_robot_status(
    use_case: RobotUseCase = Depends(get_robot_use_case),
) -> RobotStatusResponse:
    return use_case.get_status()


@router.post("/command", response_model=RobotCommandResponse)
async def send_robot_command(
    request: RobotCommandRequest,
    use_case: RobotUseCase = Depends(get_robot_use_case),
) -> RobotCommandResponse:
    return use_case.send_command(request)
