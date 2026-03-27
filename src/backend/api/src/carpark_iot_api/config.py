import os
from abc import abstractmethod, ABC
from decimal import Decimal
from typing import override, Literal

import pyrebase
from pydantic import SecretStr, Field
from pydantic_settings import BaseSettings, PydanticBaseSettingsSource, TomlConfigSettingsSource, SettingsConfigDict
from pyrebase.pyrebase import Firebase

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


class FirebaseAuthSettings(BaseSettings, ABC):
    type: str

    @abstractmethod
    def authenticate(self, firebase: Firebase) -> dict:
        raise NotImplementedError


class FirebaseEmailPasswordAuthSettings(FirebaseAuthSettings):
    type: Literal["email"] = "email"
    email: str
    password: SecretStr

    @override
    def authenticate(self, firebase: Firebase) -> dict:
        user = firebase.auth().sign_in_with_email_and_password(self.email, self.password.get_secret_value())
        return user


class FirebaseCustomTokenAuthSettings(FirebaseAuthSettings):
    type: Literal["token"] = "token"
    token: SecretStr

    @override
    def authenticate(self, firebase: Firebase) -> dict:
        user = firebase.auth().sign_in_with_custom_token(self.token.get_secret_value())
        return user


class FirebaseSettings(BaseSettings):
    api_key: SecretStr
    auth_domain: str
    db_url: str
    storage_bucket_url: str
    auth: FirebaseEmailPasswordAuthSettings | FirebaseCustomTokenAuthSettings = Field(discriminator="type")

    def create_firebase(self) -> Firebase:
        config = {
            "apiKey": self.api_key.get_secret_value(),
            "authDomain": self.auth_domain,
            "databaseURL": self.db_url,
            "storageBucket": self.storage_bucket_url
        }
        return pyrebase.initialize_app(config)


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
    "FirebaseAuthSettings",
    "FirebaseEmailPasswordAuthSettings",
    "FirebaseCustomTokenAuthSettings",
    "FirebaseSettings",
    "CarparkSettings"
)
