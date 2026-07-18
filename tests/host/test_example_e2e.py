from __future__ import annotations

import subprocess
import sys
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / "tool" / "src"))

from axiomtrace_tools.decoder import decode_stream  # noqa: E402


def main() -> int:
    output = subprocess.check_output([sys.argv[1]], text=True).strip()
    raw = bytes.fromhex(output)
    frames = decode_stream(raw)
    assert len(frames) == 1
    frame = frames[0]
    assert frame["module_id"] == 0x03
    assert frame["event_id"] == 0x0001
    assert frame["payload"] == [{"type": "raw", "value": "800c"}]

    dictionary = {
        "modules": [{
            "id": 0x03,
            "name": "motor",
            "events": [{
                "id": 0x0001,
                "name": "speed",
                "args": [{"name": "rpm", "type": "u16"}],
            }],
        }],
    }
    semantic = decode_stream(raw, dictionary)
    assert semantic[0]["payload"] == [{"type": "u16", "value": 3200}]
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
