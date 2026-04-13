import asyncio
import json
import logging
from dataclasses import dataclass
from datetime import datetime
from decimal import Decimal
from typing import Any, cast

from paho.mqtt.client import Client, ConnectFlags, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion
from paho.mqtt.properties import Properties
from paho.mqtt.reasoncodes import ReasonCode
from firebase_admin import db as firebase_db
from sqlalchemy import select

from carpark_iot_core.components.models import ParkingSpaceIndicator, SmartGate, SmartParkingSpace, \
    LicensePlateCamera, MqttComponent
from carpark_iot_core.components.schemas import PermitJoinRequest, SmartParkingSpaceInput, SmartGateOutput, \
    SmartParkingSpaceOutput
from carpark_iot_core.db.database import AsyncDatabase
from carpark_iot_core.db.models import Entry

logger = logging.getLogger(__name__)


@dataclass(slots=True, frozen=True)
class CheckoutStatus:
    license_plate: str
    price: Decimal


class Carpark:
    camera: LicensePlateCamera
    parking_space_counter: ParkingSpaceIndicator

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
            parking_space_indicator: ParkingSpaceIndicator,
            *,
            free_grace_period: int,
            price_per_hour: Decimal
    ):
        self.db = db
        self.camera = LicensePlateCamera(self.on_license_plate)
        self.parking_space_counter = parking_space_indicator
        self.free_grace_period = free_grace_period
        self.price_per_hour = price_per_hour

        self.firebase_db = firebase_db.reference("/")
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

            case ["zigbee2mqtt", friendly_name, "availability"]:
                ...

            case ["zigbee2mqtt", friendly_name]:
                payload: dict[str, Any]
                match self.mqtt_components.get(friendly_name):
                    case SmartGate() as smart_gate if friendly_name == self._exit_gate_id:
                        payload: SmartGateOutput = SmartGateOutput.model_validate(payload)

                        if payload.nfc_data:
                            asyncio.run(self.handle_nfc(payload.nfc_data))
                    case SmartParkingSpace() as smart_parking_space:
                        payload: SmartParkingSpaceInput = SmartParkingSpaceInput.model_validate(payload)
                        payload.update(smart_parking_space)

                        self.update_carpark_space_counter()

    def init_device(self, data: dict[str, Any]) -> None:
        friendly_name = data["friendly_name"]
        if friendly_name in self.mqtt_components:
            return

        match data:
            case {"model_id": model_id, "manufacturer": "Carpark"}:
                match model_id:
                    case "SGEN":
                        self.mqtt_components[friendly_name] = SmartGate(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self._entry_gate_id = friendly_name
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}/+")
                    case "SGEX":
                        self.mqtt_components[friendly_name] = SmartGate(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self._exit_gate_id = friendly_name
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}/+")
                    case "SGS3":
                        self.mqtt_components[friendly_name] = SmartParkingSpace(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}/+")
                        self.mqtt_client.publish(f"zigbee2mqtt/{friendly_name}/get", SmartParkingSpaceOutput().model_dump_json())
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

    def update_carpark_space_counter(self) -> None:
        total = remaining = 0
        for component in self.mqtt_components.values():
            if isinstance(component, SmartParkingSpace):
                total += component.total
                remaining += component.remaining

        self.parking_space_counter.display(total=total, remaining=remaining)

    async def handle_nfc(self, nfc_data: str) -> None:
        if not self.checkout:
            return

        async with self.db.session as session:
            gate = cast(SmartGate, self.mqtt_components[self._exit_gate_id])
            asyncio.create_task(gate.open_and_close(5, f"Thank you!\nCar: {self.checkout.license_plate}"))
            gate.clear_nfc()

            entry = Entry(license_plate=self.checkout.license_plate, gate_id=gate.id, type=Entry.EntryType.Exit,
                          price=self.checkout.price)
            session.add(entry)
            await session.commit()

            self.firebase_db.child("entry").set({
                "timestamp": entry.timestamp.isoformat(),
                "license_plate": entry.license_plate,
                "gate_id": entry.gate_id,
                "type": entry.type.name,
                "price": entry.price
            })


    async def handle_car(self, license_plate: str) -> None:
        stmt = select(Entry) \
            .where(Entry.license_plate == license_plate) \
            .order_by(Entry.timestamp.desc()) \
            .limit(1)

        async with self.db.session as session:
            last_entry: Entry | None = (await session.execute(stmt)).scalar_one_or_none()


            if last_entry is None or last_entry.type is Entry.EntryType.Exit:
                gate = cast(SmartGate, self.mqtt_components[self._entry_gate_id])
                asyncio.create_task(gate.open_and_close(5, f"Welcome\nCar: {license_plate}"))

                entry = Entry(license_plate=license_plate, gate_id=gate.id, type=Entry.EntryType.Entry)
                session.add(entry)
                await session.commit()

                self.firebase_db.child("entry").set({
                    "timestamp": entry.timestamp.isoformat(),
                    "license_plate": entry.license_plate,
                    "gate_id": entry.gate_id,
                    "type": entry.type.name,
                    "price": entry.price
                })
            else:
                price = self.calculate_price(entry.timestamp)  # noqa

                gate = cast(SmartGate, self.mqtt_components[self._exit_gate_id])
                if price.is_zero():
                    asyncio.create_task(gate.open_and_close(5, f"Thank you!\nCar: {self.checkout.license_plate}"))

                    entry = Entry(license_plate=license_plate, gate_id=gate.id, type=Entry.EntryType.Exit,
                                  price=price)
                    session.add(entry)
                    await session.commit()

                    self.firebase_db.child("entry").set({
                        "timestamp": entry.timestamp.isoformat(),
                        "license_plate": entry.license_plate,
                        "gate_id": entry.gate_id,
                        "type": entry.type.name,
                        "price": entry.price
                    })
                else:
                    gate.display(f"Car: {license_plate}\nPrice: ${price}")

                    self.checkout = CheckoutStatus(license_plate, price)
