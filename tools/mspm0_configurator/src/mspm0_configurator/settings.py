from __future__ import annotations

import json
import os
import tempfile
from dataclasses import asdict, dataclass, field
from math import isfinite
from numbers import Integral, Real
from pathlib import Path

from .protocol import MOTOR_COUNT, PID_LIMIT, SUPPORTED_BAUDS, TARGET_RPM_LIMIT


@dataclass
class AppSettings:
    language: str = "zh"
    baud: int = 115200
    motor: int = 0
    kp: float = 0.8
    ki: float = 0.3
    kd: float = 0.0
    target_rpm: float = 0.0
    ff_k: float = 0.0
    ff_b: float = 0.0
    ff_enabled: bool = False
    plot_window_s: int = 15
    # v0.3.0 — window geometry & sidebar state
    window_x: int = -1
    window_y: int = -1
    window_w: int = 1360
    window_h: int = 860
    window_maximized: bool = False
    left_panel_visible: bool = True
    right_panel_visible: bool = True
    main_splitter_sizes: tuple[int, ...] = (410, 900)
    telemetry_splitter_sizes: tuple[int, ...] = (780, 240)

    def __post_init__(self) -> None:
        if self.language not in {"zh", "en"}:
            raise ValueError("language must be zh or en")
        if not isinstance(self.ff_enabled, bool):
            raise ValueError("ff_enabled must be boolean")
        if not isinstance(self.window_maximized, bool):
            raise ValueError("window_maximized must be boolean")
        if not isinstance(self.left_panel_visible, bool):
            raise ValueError("left_panel_visible must be boolean")
        if not isinstance(self.right_panel_visible, bool):
            raise ValueError("right_panel_visible must be boolean")
        self.baud = self._integer(self.baud, "baud")
        self.motor = self._integer(self.motor, "motor")
        self.plot_window_s = self._integer(self.plot_window_s, "plot_window_s")
        for name in ("window_x", "window_y", "window_w", "window_h"):
            setattr(self, name, self._integer(getattr(self, name), name))
        if self.baud not in SUPPORTED_BAUDS:
            raise ValueError(f"baud must be one of {SUPPORTED_BAUDS}")
        if self.motor not in range(MOTOR_COUNT):
            raise ValueError("motor must be 0..3")
        for name in ("kp", "ki", "kd", "target_rpm", "ff_k", "ff_b"):
            value = getattr(self, name)
            if isinstance(value, bool) or not isinstance(value, Real):
                raise ValueError(f"{name} must be a number")
            value = float(value)
            if not isfinite(value):
                raise ValueError(f"{name} must be finite")
            setattr(self, name, value)
        for name in ("kp", "ki", "kd"):
            if abs(getattr(self, name)) > PID_LIMIT:
                raise ValueError(f"{name} must be in [-{PID_LIMIT}, {PID_LIMIT}]")
        if abs(self.target_rpm) > TARGET_RPM_LIMIT:
            raise ValueError(
                f"target_rpm must be in [-{TARGET_RPM_LIMIT}, {TARGET_RPM_LIMIT}]"
            )
        if not 3 <= self.plot_window_s <= 300:
            raise ValueError("plot_window_s must be 3..300")
        # Coerce splitter tuples
        self.main_splitter_sizes = tuple(int(v) for v in self.main_splitter_sizes)
        self.telemetry_splitter_sizes = tuple(int(v) for v in self.telemetry_splitter_sizes)

    @staticmethod
    def _integer(value: object, name: str) -> int:
        if isinstance(value, bool) or not isinstance(value, Integral):
            raise ValueError(f"{name} must be an integer")
        return int(value)

    def save(self, path: str | Path) -> None:
        content = json.dumps(asdict(self), indent=2, ensure_ascii=False)
        destination = Path(path)
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary: Path | None = None
        try:
            with tempfile.NamedTemporaryFile(
                "w",
                encoding="utf-8",
                dir=destination.parent,
                prefix=f".{destination.name}.",
                suffix=".tmp",
                delete=False,
            ) as stream:
                temporary = Path(stream.name)
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, destination)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)

    @classmethod
    def load(cls, path: str | Path) -> "AppSettings":
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            raise ValueError("settings file must contain a JSON object")
        allowed = cls.__dataclass_fields__.keys()
        return cls(**{key: value for key, value in data.items() if key in allowed})
