"""Protocol v1 codec shared by the GUI and unit tests."""
from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from math import isfinite
from typing import Mapping

PROTOCOL_VERSION = 1
DEFAULT_BAUD = 115200
MOTOR_COUNT = 4
PID_LIMIT = 100.0
TARGET_RPM_LIMIT = 800.0

CHANNEL_NAMES = (
    "rpm_a",
    "rpm_b",
    "rpm_c",
    "rpm_d",
    "target_a",
    "target_b",
    "target_c",
    "target_d",
    "selected_ff",
    "selected_pid_correction",
    "selected_output",
)
MOTOR_IDS = {"A": 0, "B": 1, "C": 2, "D": 3}


class LineKind(str, Enum):
    TELEMETRY = "telemetry"
    MACHINE = "machine"
    TEXT = "text"
    EMPTY = "empty"


@dataclass(frozen=True)
class TelemetryFrame:
    values: tuple[float, ...]

    def as_dict(self) -> dict[str, float]:
        return dict(zip(CHANNEL_NAMES, self.values, strict=True))


@dataclass(frozen=True)
class MachineMessage:
    topic: str
    fields: Mapping[str, str]


@dataclass(frozen=True)
class DecodedLine:
    kind: LineKind
    raw: str
    telemetry: TelemetryFrame | None = None
    machine: MachineMessage | None = None


def _number(value: float, low: float, high: float, name: str) -> str:
    value = float(value)
    if not isfinite(value) or value < low or value > high:
        raise ValueError(f"{name} must be finite and in [{low}, {high}]")
    return f"{value:.6g}"


def _motor_id(motor: int) -> int:
    motor = int(motor)
    if motor not in range(MOTOR_COUNT):
        raise ValueError("motor must be 0..3")
    return motor


class CommandBuilder:
    """Build only commands accepted by firmware protocol v1."""

    @staticmethod
    def info_query() -> str:
        return "Info?"

    @staticmethod
    def config_query() -> str:
        return "Config?"

    @staticmethod
    def status(motor: int | None = None) -> str:
        return "Status?" if motor is None else f"Status={_motor_id(motor)}"

    @staticmethod
    def stream(enabled: bool) -> str:
        return f"Stream={1 if enabled else 0}"

    @staticmethod
    def motor(motor: int) -> str:
        return f"Motor={_motor_id(motor)}"

    @staticmethod
    def pid(name: str, value: float) -> str:
        key = name.lower()
        if key not in {"kp", "ki", "kd"}:
            raise ValueError("PID name must be kp, ki or kd")
        return f"{key.capitalize()}={_number(value, -PID_LIMIT, PID_LIMIT, key)}"

    @staticmethod
    def target(rpm: float) -> str:
        return f"Target={_number(rpm, -TARGET_RPM_LIMIT, TARGET_RPM_LIMIT, 'rpm')}"

    @staticmethod
    def feedforward(k: float, b: float, enabled: bool) -> list[str]:
        return [
            f"FFk={_number(k, -10_000.0, 10_000.0, 'FFk')}",
            f"FFb={_number(b, -10_000.0, 10_000.0, 'FFb')}",
            f"FFe={1 if enabled else 0}",
        ]

    @staticmethod
    def run(motor: int) -> str:
        return f"Run={_motor_id(motor)}"

    @staticmethod
    def stop(motor: int) -> str:
        return f"Stop={_motor_id(motor)}"

    @staticmethod
    def stop_all() -> str:
        return "StopAll"

    @staticmethod
    def abort() -> str:
        return "abort"

    @staticmethod
    def mode(mode: str) -> str:
        if mode not in {"speed", "position", "angle"}:
            raise ValueError("mode must be speed, position or angle")
        return f"mode={mode}"

    @staticmethod
    def position(pulses: float, cruise_rpm: float) -> str:
        return (
            f"pos={_number(pulses, -1e9, 1e9, 'pulses')},"
            f"{_number(cruise_rpm, 0.001, TARGET_RPM_LIMIT, 'cruise_rpm')}"
        )

    @staticmethod
    def angle(degrees: float, cruise_rpm: float) -> str:
        return (
            f"angle={_number(degrees, -36_000.0, 36_000.0, 'degrees')},"
            f"{_number(cruise_rpm, 0.001, TARGET_RPM_LIMIT, 'cruise_rpm')}"
        )


def decode_line(raw: str) -> DecodedLine:
    line = raw.strip()
    if not line:
        return DecodedLine(LineKind.EMPTY, raw)

    if line.startswith("@"):
        parts = line[1:].split(",")
        topic = parts[0].strip().upper()
        if topic:
            fields: dict[str, str] = {}
            for item in parts[1:]:
                if "=" in item:
                    key, value = item.split("=", 1)
                    fields[key.strip()] = value.strip()
            return DecodedLine(
                LineKind.MACHINE,
                raw,
                machine=MachineMessage(topic, fields),
            )

    parts = line.split(",")
    if len(parts) == len(CHANNEL_NAMES):
        try:
            values = tuple(float(part) for part in parts)
        except ValueError:
            pass
        else:
            if all(isfinite(value) for value in values):
                return DecodedLine(
                    LineKind.TELEMETRY,
                    raw,
                    telemetry=TelemetryFrame(values),
                )
    return DecodedLine(LineKind.TEXT, raw)
