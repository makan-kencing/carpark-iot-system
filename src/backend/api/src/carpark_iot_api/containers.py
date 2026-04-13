import logging
import sys

from dependency_injector import containers, providers

from carpark_iot_core.core import Carpark
from carpark_iot_core.db.database import AsyncDatabase


class ApplicationContainer(containers.DeclarativeContainer):
    logging = providers.Resource(
        logging.basicConfig,
        level=logging.INFO,
        stream=sys.stdout,
    )

    # Gateways

    db: AsyncDatabase = providers.Singleton(AsyncDatabase)

    # Services

    carpark: Carpark = providers.Singleton(Carpark)

