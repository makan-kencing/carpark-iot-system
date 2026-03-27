import threading
from abc import ABC
from dataclasses import dataclass
from threading import Thread
from typing import Callable

from fast_alpr import ALPR
from gpiozero import LEDMultiCharDisplay, TrafficLights
from paho.mqtt.client import Client
from picamera2 import Picamera2, MappedArray

from carpark_iot_core.components.schemas import SmartGatePayload

alpr = ALPR(
    detector_model="yolo-v9-t-384-license-plate-end2end",
    ocr_model="cct-xs-v2-global-model",
)


class Component(ABC):
    pass


@dataclass(slots=True)
class MqttComponent(Component, ABC):
    id: str
    _mqtt_client: Client


@dataclass(slots=True)
class SmartParkingSpace(MqttComponent):
    total: int | None = None
    available: int | None = None

    @property
    def occupied(self) -> int:
        return self.total - self.available


@dataclass(slots=True)
class SmartGate(MqttComponent):
    def open(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGatePayload(gate="ON").model_dump_json(exclude_none=True))

    def close(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGatePayload(gate="OFF").model_dump_json(exclude_none=True))

    def display(self, text: str) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGatePayload(screen_text=text).model_dump_json(exclude_none=True))


@dataclass(slots=True)
class ParkingSpaceIndicator(Component):
    char: LEDMultiCharDisplay
    lights: TrafficLights

    def display(self, remaining: int, total: int) -> None:
        self.char.value = str(remaining)
        self.lights.value = (
            remaining / total > 0.66,
            0.66 >= remaining / total > 0.33,
            remaining == 0
        )


@dataclass(slots=True)
class LicensePlateCamera(Component):
    on_detect: Callable[[str], None]

    _camera: Picamera2
    _thread: Thread

    THRESHOLD = 50

    def __init__(self, on_detect: Callable[[str], None]):
        self.on_detect = on_detect

        self._camera = Picamera2()
        self._camera.configure(
            self._camera.create_preview_configuration({"size": (1024, 768)}, controls={"FrameRate": 15}))
        self._camera.start()

        self._camera.post_callback = self.draw_texts

        self._thread = threading.Thread(target=self.on_frame)

    def on_frame(self):
        image = self._camera.capture_array()

        results = alpr.predict(image)
        if not results:
            return

        for result in results:
            self.on_detect(result.ocr.text)

    @staticmethod
    def draw_texts(request):
        with MappedArray(request, "main") as m:
            alpr.draw_predictions(m.array)


__all__ = (
    "Component",
    "MqttComponent",
    "SmartParkingSpace",
    "SmartGate",
    "ParkingSpaceIndicator",
    "LicensePlateCamera"
)
