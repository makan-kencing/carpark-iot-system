import os
from decimal import Decimal
from typing import override

import firebase_admin
from firebase_admin import credentials
from pydantic import SecretStr, FilePath
from pydantic_settings import BaseSettings, PydanticBaseSettingsSource, TomlConfigSettingsSource, SettingsConfigDict

from carpark_iot_core.components.config import ParkingSpaceIndicatorConfig


class MqttConfig(BaseSettings):
    host: str
    port: int = 1883


class DatabaseSettings(BaseSettings):
    driver: str = "timescaledb+asyncpg"
    host: str
    port: int
    name: str
    user: str
    password: SecretStr

    @property
    def connection_url(self) -> SecretStr:
        return SecretStr(
            f"{self.driver}://{self.user}:{self.password.get_secret_value()}@{self.host}:{self.port}/{self.name}"
        )


class FirebaseSettings(BaseSettings):
    service_account_path: FilePath
    db_url: str
    storage_bucket_url: str

    def auth(self) -> None:
        cert = credentials.Certificate(str(self.service_account_path))
        firebase_admin.initialize_app(cert, {
            "databaseURL": self.db_url,
            "storageBucket": self.storage_bucket_url
        })


class CarparkSettings(BaseSettings):
    mqtt: MqttConfig
    db: DatabaseSettings
    firebase: FirebaseSettings
    indicator: ParkingSpaceIndicatorConfig

    free_grace_period: int = 5 * 60
    price_per_hour: Decimal = Decimal(1)

    model_config = SettingsConfigDict(toml_file=os.getenv("CONFIG_PATH"))

    @override
    @classmethod
    def settings_customise_sources(
            cls,
            settings_cls: type[BaseSettings],
            init_settings: PydanticBaseSettingsSource,
            env_settings: PydanticBaseSettingsSource,
            dotenv_settings: PydanticBaseSettingsSource,
            file_secret_settings: PydanticBaseSettingsSource,
    ) -> tuple[PydanticBaseSettingsSource, ...]:
        return (
            init_settings,
            env_settings,
            dotenv_settings,
            file_secret_settings,
            TomlConfigSettingsSource(settings_cls)
        )


__all__ = (
    "MqttConfig",
    "DatabaseSettings",
    "FirebaseSettings",
    "CarparkSettings"
)
