import asyncio
import json
import logging
from dataclasses import dataclass
from datetime import datetime
from decimal import Decimal
from typing import Any, cast, Callable

from paho.mqtt.client import Client, ConnectFlags, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion
from paho.mqtt.properties import Properties
from paho.mqtt.reasoncodes import ReasonCode
from pyrebase.pyrebase import Firebase
from sqlalchemy import select

from carpark_iot_core.components.models import ParkingSpaceIndicator, SmartGate, SmartParkingSpace, \
    LicensePlateCamera, MqttComponent
from carpark_iot_core.components.schemas import PermitJoinRequest, SmartParkingSpacePayload, SmartGatePayload
from carpark_iot_core.db.database import AsyncDatabase
from carpark_iot_core.db.models import Entry

logger = logging.getLogger(__name__)


@dataclass(slots=True, frozen=True)
class CheckoutStatus:
    license_plate: str
    price: Decimal


class Carpark:
    camera: LicensePlateCamera
    parking_space_indicator: ParkingSpaceIndicator

    free_grace_period: int
    price_per_hour: Decimal

    mqtt_components: dict[str, MqttComponent] = {}
    checkout: CheckoutStatus | None = None

    _entry_gate_id: str
    _exit_gate_id: str

    def __init__(
            self,
            db: AsyncDatabase,
            mqtt_host: str,
            mqtt_port: int,
            firebase_client: Firebase,
            firebase_refresh: Callable[[Firebase], None],
            parking_space_indicator: ParkingSpaceIndicator,
            *,
            free_grace_period: int,
            price_per_hour: Decimal
    ):
        self.db = db
        self.firebase_client = firebase_client
        self.firebase_refresh = firebase_refresh
        self.parking_space_indicator = parking_space_indicator
        self.free_grace_period = free_grace_period
        self.price_per_hour = price_per_hour

        self.firebase_refresh(self.firebase_client)

        self.mqtt_client = Client(CallbackAPIVersion.VERSION2)
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        self.mqtt_client.connect_async(mqtt_host, mqtt_port)
        self.mqtt_client.loop_start()

    def on_license_plate(self, license_plate: str) -> None:
        asyncio.run(self.handle_car(license_plate))

    def on_mqtt_connect(self, client: Client, userdata: Any, flags: ConnectFlags, reason: ReasonCode,
                        properties: Properties | None) -> None:
        logger.info(f"Mqtt connected with result code {reason}")
        self.mqtt_client.subscribe("zigbee2mqtt/bridge/devices")

        self.permit_join()

    def on_mqtt_message(self, client: Client, userdata: Any, message: MQTTMessage) -> None:
        payload = json.loads(message.payload)
        match message.topic.split("/"):
            case ["zigbee2mqtt", "bridge", "devices"]:
                payload: list[dict[str, Any]]
                for device in payload:
                    self.init_device(device)

            case ["zigbee2mqtt", friendly_name]:
                payload: dict[str, Any]
                match self.mqtt_components.get(friendly_name):
                    case SmartGate() if friendly_name == self._exit_gate_id:
                        payload: SmartGatePayload = SmartGatePayload.model_validate(payload)

                        if payload.nfc_data:
                            asyncio.run(self.handle_nfc())
                    case SmartParkingSpace():
                        payload: SmartParkingSpacePayload = SmartParkingSpacePayload.model_validate(payload)

                        parking_space = cast(SmartParkingSpace, self.mqtt_components[friendly_name])
                        if payload.total:
                            parking_space.total = payload.total
                        if payload.available:
                            parking_space.available = payload.available

    def init_device(self, data: dict[str, Any]) -> None:
        friendly_name = data["friendly_name"]
        if friendly_name in self.mqtt_components:
            return

        match data:
            case {"model_id": model_id, "manufacturer": "carpark"}:
                match model_id:
                    case "smart_gate_entry":
                        self.mqtt_components[friendly_name] = SmartGate(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self._entry_gate_id = friendly_name
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}")
                    case "smart_gate_exit":
                        self.mqtt_components[friendly_name] = SmartGate(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self._exit_gate_id = friendly_name
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}")
                    case "smart_parking_space":
                        self.mqtt_components[friendly_name] = SmartParkingSpace(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}")
                        self.mqtt_client.publish(f"zigbee2mqtt/{friendly_name}/get",
                                                 SmartParkingSpacePayload().model_dump_json())
            case _:
                return

    def permit_join(self, duration: int = 300) -> None:
        self.mqtt_client.publish(
            "zigbee2mqtt/bridge/request/permit_join",
            PermitJoinRequest(time=duration).model_dump_json(exclude_none=True)
        )

    def calculate_price(self, last_timestamp: datetime) -> Decimal:
        price = Decimal(0)

        delta_seconds = (datetime.now() - last_timestamp).total_seconds()
        if delta_seconds < self.free_grace_period:
            return price

        price += self.price_per_hour * int(delta_seconds // 3600)

        return price

    async def handle_nfc(self):
        if not self.checkout:
            return

        async with self.db.session as session:
            gate = cast(SmartGate, self.mqtt_components[self._exit_gate_id])
            gate.open()
            gate.display(f"Thank you!\nCar: {self.checkout.license_plate}")

            entry = Entry(license_plate=self.checkout.license_plate, gate_id=gate.id, type=Entry.EntryType.Exit,
                          price=self.checkout.price)
            session.add(entry)
            await session.commit()

    async def handle_car(self, license_plate: str):
        stmt = select(Entry) \
            .where(Entry.license_plate == license_plate) \
            .order_by(Entry.timestamp.desc()) \
            .limit(1)

        async with self.db.session as session:
            entry: Entry | None = (await session.execute(stmt)).scalar_one_or_none()

            match entry.type:
                case Entry.EntryType.Entry:
                    price = self.calculate_price(entry.timestamp)  # noqa

                    gate = cast(SmartGate, self.mqtt_components[self._exit_gate_id])
                    if price.is_zero():
                        gate.open()
                        gate.display(f"Thank you!\nCar: {license_plate}")

                        entry = Entry(license_plate=license_plate, gate_id=gate.id, type=Entry.EntryType.Exit,
                                      price=price)
                        session.add(entry)
                        await session.commit()
                    else:
                        gate.display(f"Car: {license_plate}\nPrice: ${price}")

                        self.checkout = CheckoutStatus(license_plate, price)
                case Entry.EntryType.Exit | None:
                    gate = cast(SmartGate, self.mqtt_components[self._entry_gate_id])
                    gate.open()
                    gate.display(f"Welcome\nCar: {license_plate}")

                    entry = Entry(license_plate=license_plate, gate_id=gate.id, type=Entry.EntryType.Entry)
                    session.add(entry)
                    await session.commit()
