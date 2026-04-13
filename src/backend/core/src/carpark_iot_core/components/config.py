from abc import abstractmethod
from typing import Protocol, Sequence, override
from dataclasses import dataclass

from gpiozero import LEDCharDisplay, LEDMultiCharDisplay, TrafficLights

from carpark_iot_core.components.models import Component, ParkingSpaceIndicator


class ComponentConfig[T: Component](Protocol):
    @abstractmethod
    def construct(self) -> T:
        raise NotImplementedError()


@dataclass(slots=True)
class ParkingSpaceIndicatorConfig(ComponentConfig[ParkingSpaceIndicator]):
    a: int
    b: int
    c: int
    d: int
    e: int
    f: int
    g: int
    dp: int

    pins: Sequence[int]

    red: int
    amber: int
    green: int

    @override
    def construct(self) -> ParkingSpaceIndicator:
        return ParkingSpaceIndicator(
            LEDMultiCharDisplay(
                LEDCharDisplay(self.a, self.b, self.c, self.d, self.e, self.f, self.g, dp=self.dp),
                self.pins
            ),
            TrafficLights(self.red, self.amber, self.green)
        )

__all__ = (
    "ComponentConfig",
    "ParkingSpaceIndicatorConfig"
)
