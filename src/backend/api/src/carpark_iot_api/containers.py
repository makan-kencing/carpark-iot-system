import logging
import sys

from dependency_injector import containers, providers

from carpark_iot_core.core import Carpark
from carpark_iot_core.db.database import AsyncDatabase


class ApplicationContainer(containers.DeclarativeContainer):
    config: providers.Configuration = providers.Configuration()

    logging = providers.Resource(
        logging.basicConfig,
        level=logging.INFO,
        stream=sys.stdout,
    )

    # Gateways

    db: AsyncDatabase = providers.Singleton(
        AsyncDatabase,
        db_url=config.db.as_(lambda o: f"{o.driver}://{o.user}:{o.password.get_secret_value()}@{o.host}:{o.port}/{o.name}")
    )

    # Services

    carpark: Carpark = providers.Singleton(
        Carpark,
        db=db,
        mqtt_host=config.mqtt.host,
        mqtt_port=config.mqtt.port,
        parking_space_indicator=config.indicator.as_(lambda c: c.construct()),
        free_grace_period=config.free_grace_period,
        price_per_hour=config.price_per_hour
    )

