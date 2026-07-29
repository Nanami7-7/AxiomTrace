"""Protocol v1 codec shared by the GUI and unit tests."""
from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from math import isfinite
from numbers import Integral
from typing import Mapping

PROTOCOL_VERSION = 1
DEFAULT_BAUD = 115200
SUPPORTED_BAUDS = (DEFAULT_BAUD,)
MOTOR_COUNT = 4
PID_LIMIT = 100.0
TARGET_RPM_LIMIT = 800.0
PLANNER_RPM_LIMIT = 300.0

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
class DeviceInfo:
    firmware: str
    protocol: int
    board: str
    driver: str
    motors: int
    baud: int
    telemetry: str


@dataclass(frozen=True)
class MotorStatus:
    motor: int
    enabled: bool
    power: bool
    rpm: float
    target: float
    output: float
    kp: float
    ki: float
    kd: float
    ff_enabled: bool
    ff_k: float
    ff_b: float
    mode: str


@dataclass(frozen=True)
class DecodedLine:
    kind: LineKind
    raw: str
    telemetry: TelemetryFrame | None = None
    machine: MachineMessage | None = None


def _field(message: MachineMessage, name: str) -> str:
    try:
        return message.fields[name]
    except KeyError as exc:
        raise ValueError(f"@{message.topic} is missing {name}") from exc


def _field_int(message: MachineMessage, name: str) -> int:
    raw = _field(message, name)
    try:
        return int(raw, 10)
    except ValueError as exc:
        raise ValueError(f"@{message.topic} {name} must be an integer") from exc


def _field_float(message: MachineMessage, name: str) -> float:
    raw = _field(message, name)
    try:
        value = float(raw)
    except ValueError as exc:
        raise ValueError(f"@{message.topic} {name} must be a number") from exc
    if not isfinite(value):
        raise ValueError(f"@{message.topic} {name} must be finite")
    return value


def _field_bool(message: MachineMessage, name: str) -> bool:
    value = _field_int(message, name)
    if value not in (0, 1):
        raise ValueError(f"@{message.topic} {name} must be 0 or 1")
    return bool(value)


def parse_device_info(message: MachineMessage) -> DeviceInfo:
    if message.topic != "INFO":
        raise ValueError("expected @INFO")
    motors = _field_int(message, "motors")
    if motors <= 0:
        raise ValueError("@INFO motors must be positive")
    baud = _field_int(message, "baud")
    if baud <= 0:
        raise ValueError("@INFO baud must be positive")
    return DeviceInfo(
        firmware=_field(message, "fw"),
        protocol=_field_int(message, "proto"),
        board=_field(message, "board"),
        driver=_field(message, "driver"),
        motors=motors,
        baud=baud,
        telemetry=_field(message, "telemetry"),
    )


def parse_motor_status(message: MachineMessage) -> MotorStatus:
    if message.topic != "STATUS":
        raise ValueError("expected @STATUS")
    motor = _motor_id(_field_int(message, "motor"))
    mode = _field(message, "mode")
    if mode not in {"speed", "position", "angle"}:
        raise ValueError("@STATUS mode is unsupported")
    return MotorStatus(
        motor=motor,
        enabled=_field_bool(message, "enabled"),
        power=_field_bool(message, "power"),
        rpm=_field_float(message, "rpm"),
        target=_field_float(message, "target"),
        output=_field_float(message, "output"),
        kp=_field_float(message, "kp"),
        ki=_field_float(message, "ki"),
        kd=_field_float(message, "kd"),
        ff_enabled=_field_bool(message, "ff_en"),
        ff_k=_field_float(message, "ff_k"),
        ff_b=_field_float(message, "ff_b"),
        mode=mode,
    )


def _number(value: float, low: float, high: float, name: str) -> str:
    value = float(value)
    if not isfinite(value) or value < low or value > high:
        raise ValueError(f"{name} must be finite and in [{low}, {high}]")
    return f"{value:.6g}"


def _motor_id(motor: int) -> int:
    if isinstance(motor, bool) or not isinstance(motor, Integral):
        raise ValueError("motor must be an integer in 0..3")
    motor = int(motor)
    if motor not in range(MOTOR_COUNT):
        raise ValueError("motor must be 0..3")
    return motor


def encode_command(command: str) -> bytes:
    """Encode exactly one ASCII protocol command with the MCU line ending."""
    command = command.strip()
    if not command:
        raise ValueError("command must not be empty")
    if "\r" in command or "\n" in command:
        raise ValueError("command must contain exactly one protocol line")
    return (command + "\r\n").encode("ascii", errors="strict")


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
            f"{_number(cruise_rpm, 0.001, PLANNER_RPM_LIMIT, 'cruise_rpm')}"
        )

    @staticmethod
    def angle(degrees: float, cruise_rpm: float) -> str:
        return (
            f"angle={_number(degrees, -36_000.0, 36_000.0, 'degrees')},"
            f"{_number(cruise_rpm, 0.001, PLANNER_RPM_LIMIT, 'cruise_rpm')}"
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
