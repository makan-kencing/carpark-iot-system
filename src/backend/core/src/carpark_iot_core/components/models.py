import logging
import os
import statistics
import threading
import time
from abc import ABC
from dataclasses import dataclass
from threading import Thread
from typing import Callable, Literal

import cv2
from fast_alpr import ALPR, ALPRResult
from gpiozero import LEDMultiCharDisplay, TrafficLights
from libcamera import controls, Transform
from paho.mqtt.client import Client
from picamera2 import Picamera2, MappedArray, Preview

from carpark_iot_core.components.schemas import SmartGateOutput, State, SmartParkingSpaceOutput

alpr = ALPR(
    detector_model="yolo-v9-t-384-license-plate-end2end",
    ocr_model="cct-xs-v2-global-model",
)

logger = logging.getLogger("uvicorn.error")


class Component(ABC):
    pass


@dataclass(slots=True)
class MqttComponent(Component, ABC):
    id: str
    _mqtt_client: Client


@dataclass(slots=True)
class SmartParkingSpace(MqttComponent):
    total: int | None = None
    remaining: int | None = None

    @property
    def occupied(self) -> int:
        return self.total - self.remaining

    def fetch_info(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/get", SmartParkingSpaceOutput().model_dump_json())


@dataclass(slots=True)
class SmartGate(MqttComponent):
    status: State = "OFF"

    def open(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGateOutput(gate="ON").model_dump_json(exclude_none=True))

    def close(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGateOutput(gate="OFF").model_dump_json(exclude_none=True))

    def display(self, text: str) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGateOutput(display_text=text).model_dump_json(exclude_none=True))

    def clear_nfc(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGateOutput(clear_nfc=True).model_dump_json(exclude_none=True))

    def clear_display(self) -> None:
        self._mqtt_client.publish(f"zigbee2mqtt/{self.id}/set",
                                  SmartGateOutput(clear_display=True).model_dump_json(exclude_none=True))

    def open_and_close(self, delay: int, open_text: str | None = None) -> None:
        self.open()
        if open_text:
            self.display(open_text)

        time.sleep(delay)

        self.close()
        self.clear_display()


@dataclass(slots=True)
class ParkingSpaceIndicator(Component):
    char: LEDMultiCharDisplay
    lights: TrafficLights

    def display(self, remaining: int, total: int) -> None:
        self.char.value = str(remaining)
        self.lights.value = (
            remaining == 0,
            0.66 >= remaining / total > 0.33,
            remaining / total > 0.66
        )


@dataclass(slots=True)
class LicensePlateCamera(Component):
    on_detect: Callable[[str], None]

    min_threshold: float
    _camera: Picamera2
    _thread: Thread
    _predictions: list[ALPRResult]

    THRESHOLD = 50

    def __init__(self, on_detect: Callable[[str], None], *, min_threshold: float = 0.95):
        self.on_detect = on_detect
        self.min_threshold = min_threshold

        self._camera = Picamera2()
        camera_config = self._camera.create_preview_configuration(
            {"size": (1024, 768)},
            transform=Transform(hflip=1, vflip=1),
            controls={"FrameRate": 10, "AfMode": controls.AfModeEnum.Continuous}
        )
        self._camera.configure(camera_config)
        if "DISPLAY" in os.environ:
            self._camera.start_preview(Preview.QTGL)
        self._camera.start()

        self._camera.post_callback = self.draw_texts

        self._predictions: list[ALPRResult] = []
        self._thread = threading.Thread(target=self.frame_loop, daemon=True)
        self._thread.start()

    def check_confidence(self, result: ALPRResult) -> bool:
        return (statistics.mean(result.ocr.confidence)
                if isinstance(result.ocr.confidence, list)
                else result.ocr.confidence) > self.min_threshold

    def frame_loop(self):
        while True:
            image = self._camera.capture_array()
            predictions: list[ALPRResult] = alpr.predict(image[:, :, 0:3])
            predictions = list(filter(lambda p: self.check_confidence(p) and p.ocr.text, predictions))
            if predictions:
                for result in predictions:
                    for old_result in self._predictions:
                        if result.ocr.text == old_result.ocr.text:
                            break
                    else:
                        try:
                            self.on_detect(result.ocr.text)
                            time.sleep(5)
                        except Exception:
                            logger.exception("Exception while calling camera callback")

            self._predictions = predictions

            time.sleep(0.2)

    def draw_texts(self, request):
        with MappedArray(request, "main") as m:
            if not self._predictions:
                cv2.rectangle(m.array, (0, 0), (0, 0), (36, 255, 12), 0)

            for result in self._predictions:
                detection = result.detection
                ocr_result = result.ocr
                bbox = detection.bounding_box
                x1, y1, x2, y2 = bbox.x1, bbox.y1, bbox.x2, bbox.y2
                # Draw the bounding box
                cv2.rectangle(m.array, (x1, y1), (x2, y2), (36, 255, 12), 2)
                if ocr_result is None or not ocr_result.text or not ocr_result.confidence:
                    continue
                confidence: float = (
                    statistics.mean(ocr_result.confidence)
                    if isinstance(ocr_result.confidence, list)
                    else ocr_result.confidence
                )
                font_scale = min(1.25, max(0.4, m.array.shape[1] / 1000))
                text_thickness = 1 if font_scale < 0.75 else 2
                outline_thickness = text_thickness + max(3, round(font_scale * 3))
                display_lines = [f"{ocr_result.text} {confidence * 100:.0f}%"]
                if ocr_result.region:
                    region_text = ocr_result.region
                    if ocr_result.region_confidence is not None:
                        region_text = f"{region_text} {ocr_result.region_confidence * 100:.0f}%"
                    display_lines.insert(0, region_text)

                _, text_height = cv2.getTextSize(
                    display_lines[0], cv2.FONT_HERSHEY_SIMPLEX, font_scale, text_thickness
                )[0]
                line_gap = max(14, round(text_height * 0.6))
                line_height = text_height + line_gap
                text_y = y1 - 10 - ((len(display_lines) - 1) * line_height)
                if text_y - text_height < 0:
                    text_y = y2 + text_height + 10

                for idx, line in enumerate(display_lines):
                    text_width, current_text_height = cv2.getTextSize(
                        line, cv2.FONT_HERSHEY_SIMPLEX, font_scale, text_thickness
                    )[0]
                    text_x = min(max(x1, 5), max(5, m.array.shape[1] - text_width - 5))
                    current_y = min(
                        max(text_y + (idx * line_height), current_text_height + 5),
                        m.array.shape[0] - 5,
                    )
                    # Draw black background for better readability
                    cv2.putText(
                        img=m.array,
                        text=line,
                        org=(text_x, current_y),
                        fontFace=cv2.FONT_HERSHEY_SIMPLEX,
                        fontScale=font_scale,
                        color=(0, 0, 0),
                        thickness=outline_thickness,
                        lineType=cv2.LINE_AA,
                    )
                    # Draw white text
                    cv2.putText(
                        img=m.array,
                        text=line,
                        org=(text_x, current_y),
                        fontFace=cv2.FONT_HERSHEY_SIMPLEX,
                        fontScale=font_scale,
                        color=(255, 255, 255),
                        thickness=text_thickness,
                        lineType=cv2.LINE_AA,
                    )


__all__ = (
    "Component",
    "MqttComponent",
    "SmartParkingSpace",
    "SmartGate",
    "ParkingSpaceIndicator",
    "LicensePlateCamera"
)
