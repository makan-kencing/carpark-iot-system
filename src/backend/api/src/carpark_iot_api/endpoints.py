from dependency_injector.wiring import inject
from fastapi import APIRouter
from fastapi.templating import Jinja2Templates
from starlette.staticfiles import StaticFiles

router = APIRouter()
router.mount("/static", StaticFiles(directory="static"), name="static")

templates = Jinja2Templates(directory="templates")

@router.get("/")
@inject
async def index():
    ...


__all__ = (
    "router",
)