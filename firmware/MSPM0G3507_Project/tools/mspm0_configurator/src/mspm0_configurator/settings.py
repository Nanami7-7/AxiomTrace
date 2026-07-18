from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from math import isfinite
from pathlib import Path


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

    def __post_init__(self) -> None:
        if self.language not in {"zh", "en"}:
            raise ValueError("language must be zh or en")
        if not 1_200 <= int(self.baud) <= 3_000_000:
            raise ValueError("baud is outside the supported range")
        if int(self.motor) not in range(4):
            raise ValueError("motor must be 0..3")
        for name in ("kp", "ki", "kd", "target_rpm", "ff_k", "ff_b"):
            if not isfinite(float(getattr(self, name))):
                raise ValueError(f"{name} must be finite")
        if not 3 <= int(self.plot_window_s) <= 300:
            raise ValueError("plot_window_s must be 3..300")

    def save(self, path: str | Path) -> None:
        content = json.dumps(asdict(self), indent=2, ensure_ascii=False)
        Path(path).write_text(content, encoding="utf-8")

    @classmethod
    def load(cls, path: str | Path) -> "AppSettings":
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            raise ValueError("settings file must contain a JSON object")
        allowed = cls.__dataclass_fields__.keys()
        return cls(**{key: value for key, value in data.items() if key in allowed})
