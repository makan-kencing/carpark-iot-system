import logging
import sys

from dependency_injector import containers, providers

from carpark_iot_core.core import Carpark
from carpark_iot_core.db.database import Database


class ApplicationContainer(containers.DeclarativeContainer):
    logging = providers.Resource(
        logging.basicConfig,
        level=logging.INFO,
        stream=sys.stdout,
    )

    # Gateways

    db: Database = providers.Dependency(instance_of=Database)

    # Services

    carpark: Carpark = providers.Dependency(instance_of=Carpark)

