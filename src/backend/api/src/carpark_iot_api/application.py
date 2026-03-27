from fastapi import FastAPI

from carpark_iot_api import endpoints
from carpark_iot_api.config import CarparkSettings
from carpark_iot_api.containers import ApplicationContainer


def create_app() -> FastAPI:
    container = ApplicationContainer()
    container.wire(modules=[__name__])

    container.config.from_pydantic(CarparkSettings())  # noqa

    app = FastAPI()
    app.container = container
    app.include_router(endpoints.router)
    return app


if __name__ == '__main__':
    create_app()
