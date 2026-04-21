import json
import logging
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timedelta
from decimal import Decimal
from typing import Any, cast

from firebase_admin import db as firebase_db
from paho.mqtt.client import Client, ConnectFlags, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion
from paho.mqtt.properties import Properties
from paho.mqtt.reasoncodes import ReasonCode
from sqlalchemy import select

from carpark_iot_core.components.models import ParkingSpaceIndicator, SmartGate, SmartParkingSpace, \
    LicensePlateCamera, MqttComponent
from carpark_iot_core.components.schemas import PermitJoinRequest, SmartParkingSpaceInput, \
    SmartParkingSpaceOutput, SmartGateInput
from carpark_iot_core.db.database import Database
from carpark_iot_core.db.models import Entry, Wallet

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

    _entry_gate_id: str | None
    _exit_gate_id: str | None

    def __init__(
            self,
            db: Database,
            mqtt_host: str,
            mqtt_port: int,
            parking_space_indicator: ParkingSpaceIndicator,
            *,
            free_grace_period: int,
            price_per_hour: Decimal
    ):
        self.db = db
        self.camera = LicensePlateCamera(self.handle_car)
        self.parking_space_counter = parking_space_indicator
        self.free_grace_period = free_grace_period
        self.price_per_hour = price_per_hour
        self._entry_gate_id = None
        self._exit_gate_id = None

        self.firebase_db = firebase_db.reference("/")
        threading.Thread(target=self.firebase_db.child("gate/entry/state").listen,
                         args=(lambda e: self._handle_entry_gate_firebase(self._entry_gate_id or "", e),)).start()
        threading.Thread(target=self.firebase_db.child("gate/exit/state").listen,
                         args=(lambda e: self._handle_entry_gate_firebase(self._exit_gate_id or "", e),)).start()

        self.mqtt_client = Client(CallbackAPIVersion.VERSION2)
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        self.mqtt_client.connect_async(mqtt_host, mqtt_port)
        self.mqtt_client.loop_start()

        self.parking_space_counter.display(0, 1)

    @property
    def parking_spaces(self) -> tuple[int, int]:
        total = 0
        remaining = 0
        for component in self.mqtt_components.values():
            if isinstance(component, SmartParkingSpace):
                total += component.total
                remaining += component.remaining
        return total, remaining

    def _handle_entry_gate_firebase(self, gate_id: str, event: firebase_db.Event):
        gate: MqttComponent | None = self.mqtt_components.get(gate_id)
        if gate is None:
            return
        gate: SmartGate

        if json.loads(event.data):
            gate.open()
        else:
            gate.close()

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
                        payload: SmartGateInput = SmartGateInput.model_validate(payload)
                        payload.update(smart_gate)

                        if payload.nfc_data:
                            self.handle_nfc(bytes.fromhex(payload.nfc_data))
                    case SmartParkingSpace() as smart_parking_space:
                        payload: SmartParkingSpaceInput = SmartParkingSpaceInput.model_validate(payload)
                        payload.update(smart_parking_space)

                        self.update_carpark_space_counter()

    def init_device(self, data: dict[str, Any]) -> None:
        friendly_name = data["friendly_name"]
        if friendly_name in self.mqtt_components:
            return

        match data:
            case {"model_id": model_id, "manufacturer": "ESPRESSIF"}:
                match model_id:
                    case "SGEN":
                        self.mqtt_components[friendly_name] = SmartGate(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self._entry_gate_id = friendly_name
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}")
                    case "SGEX":
                        self.mqtt_components[friendly_name] = SmartGate(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )
                        self._exit_gate_id = friendly_name
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}")
                    case "SPS3":
                        component = SmartParkingSpace(
                            id=friendly_name,
                            _mqtt_client=self.mqtt_client
                        )

                        self.mqtt_components[friendly_name] = component
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}")
                        self.mqtt_client.subscribe(f"zigbee2mqtt/{friendly_name}/get")
                        threading.Thread(target=self.fetch_parking_space, args=(friendly_name,)).start()
            case _:
                return

    def fetch_parking_space(self, component_id) -> None:
        while component_id in self.mqtt_components:
            cast(SmartParkingSpace, self.mqtt_components[component_id]).fetch_info()
            time.sleep(1)


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

        price += self.price_per_hour * Decimal(delta_seconds / 3600)

        return price

    def update_carpark_space_counter(self) -> None:
        total, remaining = self.parking_spaces
        self.parking_space_counter.display(total=total, remaining=remaining)

    def handle_nfc(self, nfc_id: bytes) -> None:
        if self._exit_gate_id is None:
            return

        if not self.checkout:
            return

        with self.db.session as session:
            stmt = select(Wallet).where(Wallet.nfc == nfc_id)
            wallet: Wallet | None = session.scalars(stmt).one_or_none()
            if wallet is None:
                return

            gate = cast(SmartGate, self.mqtt_components[self._exit_gate_id])
            if wallet.balance < self.checkout.price:
                gate.display(
                    f"Car: {self.checkout.license_plate}\nPrice: ${self.checkout.price}\nInsufficient\nbalance")
                return

            wallet.balance -= self.checkout.price
            threading.Thread(target=gate.open_and_close, args=(
                5,
                f"Thank you!\nCar: {self.checkout.license_plate}\nPrice: RM {self.checkout.price}\nRemaining: RM {wallet.balance}"
            )).start()
            gate.clear_nfc()

            entry = Entry(license_plate=self.checkout.license_plate, gate_id=gate.id, type=Entry.EntryType.Exit,
                          wallet_id=wallet.id, price=self.checkout.price)
            session.add(entry)
            session.commit()

            threading.Thread(
                target=self.firebase_db.child("entry").push,
                args=({
                          "timestamp": entry.timestamp.isoformat(),
                          "license_plate": entry.license_plate,
                          "gate_id": entry.gate_id,
                          "type": entry.type.name,
                          "price": str(entry.price)
                      },)
            )

    def handle_car(self, license_plate: str) -> None:
        if self.parking_spaces[1] == 0:
            return

        stmt = select(Entry) \
            .where(Entry.license_plate == license_plate) \
            .order_by(Entry.timestamp.desc()) \
            .limit(1)

        with self.db.session as session:
            last_entry: Entry | None = session.scalars(stmt).one_or_none()

            if last_entry is not None and (datetime.now() - last_entry.timestamp).total_seconds() < 5:
                return

            if last_entry is None or last_entry.type is Entry.EntryType.Exit:
                if self._entry_gate_id is None:
                    return

                gate = cast(SmartGate, self.mqtt_components[self._entry_gate_id])

                threading.Thread(target=gate.open_and_close, args=(5, f"Welcome\nCar: {license_plate}")).start()

                entry = Entry(license_plate=license_plate, gate_id=gate.id, type=Entry.EntryType.Entry)
                session.add(entry)
                session.commit()

                threading.Thread(
                    target=self.firebase_db.child("entry").push,
                    args=({
                              "timestamp": entry.timestamp.isoformat(),
                              "license_plate": entry.license_plate,
                              "gate_id": entry.gate_id,
                              "type": entry.type.name
                          },)
                )
            else:
                if self._exit_gate_id is None:
                    return

                price = self.calculate_price(last_entry.timestamp)

                gate = cast(SmartGate, self.mqtt_components[self._exit_gate_id])
                if price.is_zero():
                    threading.Thread(target=gate.open_and_close,
                                     args=(5, f"Thank you!\nCar: {last_entry.license_plate}")).start()

                    entry = Entry(license_plate=license_plate, gate_id=gate.id, type=Entry.EntryType.Exit,
                                  price=price)
                    session.add(entry)
                    session.commit()

                    threading.Thread(
                        target=self.firebase_db.child("entry").push,
                        args=({
                                  "timestamp": entry.timestamp.isoformat(),
                                  "license_plate": entry.license_plate,
                                  "gate_id": entry.gate_id,
                                  "type": entry.type.name,
                                  "price": str(entry.price)
                              },)
                    )
                else:
                    gate.display(f"Car: {license_plate}\nPrice: ${price}")

                    self.checkout = CheckoutStatus(license_plate, price)
