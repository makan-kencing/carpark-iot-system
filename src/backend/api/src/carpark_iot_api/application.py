from contextlib import asynccontextmanager

from dependency_injector import providers
from fastapi import FastAPI
from picamera2.encoders import JpegEncoder
from picamera2.outputs import FileOutput

from carpark_iot_api import endpoints
from carpark_iot_api.config import CarparkSettings
from carpark_iot_api.containers import ApplicationContainer
from carpark_iot_core.core import Carpark
from carpark_iot_core.db.database import AsyncDatabase


@asynccontextmanager
async def lifespan(app: FastAPI):
    await app.container.db().create_database()  # noqa
    yield


def create_app() -> FastAPI:
    settings = CarparkSettings()  # noqa
    settings.firebase.auth()

    db = AsyncDatabase(db_url=settings.db.connection_url.get_secret_value())

    carpark = Carpark(
        db=db,
        mqtt_host=settings.mqtt.host,
        mqtt_port=settings.mqtt.port,
        parking_space_indicator=settings.indicator.construct(),
        free_grace_period=settings.free_grace_period,
        price_per_hour=settings.price_per_hour
    )

    carpark.camera._camera.start_recording(JpegEncoder(), FileOutput(endpoints.output))

    container = ApplicationContainer(db=providers.Object(db), carpark=providers.Object(carpark))
    container.wire(modules=[endpoints])

    app = FastAPI(lifespan=lifespan)
    app.container = container
    app.include_router(endpoints.router)
    return app


if __name__ == '__main__':
    app = create_app()
