from fastapi import FastAPI

from carpark_iot_api import endpoints
from carpark_iot_api.config import CarparkSettings
from carpark_iot_api.containers import ApplicationContainer


def create_app() -> FastAPI:
    container = ApplicationContainer()
    container.wire(modules=[__name__])

    settings = CarparkSettings()  # noqa
    settings.firebase.auth()

    container.config.from_pydantic(settings)
    container.config.db.connection_url = settings.db.connection_url
    container.carpark()  # noqa

    app = FastAPI()
    app.container = container
    app.include_router(endpoints.router)
    return app


if __name__ == '__main__':
    create_app()
