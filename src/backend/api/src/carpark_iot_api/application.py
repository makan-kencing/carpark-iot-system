from fastapi import FastAPI
from picamera2.encoders import MJPEGEncoder
from picamera2.outputs import FileOutput

from carpark_iot_api import endpoints
from carpark_iot_api.config import CarparkSettings
from carpark_iot_api.containers import ApplicationContainer
from carpark_iot_core.core import Carpark
from carpark_iot_core.db.database import AsyncDatabase


def create_app() -> FastAPI:
    container = ApplicationContainer()
    container.wire(modules=[endpoints])

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

    carpark.camera._camera.start_recording(MJPEGEncoder(), FileOutput(endpoints.output))

    container.db = db
    container.carpark = carpark

    app = FastAPI()
    app.container = container
    app.include_router(endpoints.router)
    return app


if __name__ == '__main__':
    app = create_app()
