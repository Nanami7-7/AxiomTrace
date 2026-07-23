from __future__ import annotations

import queue
import threading
import time
from collections.abc import Iterable

from PySide6.QtCore import QThread, Signal

from .protocol import encode_command


class SerialWorker(QThread):
    """Own the serial port in a worker thread and exchange complete text lines."""

    MAX_PENDING_COMMANDS = 256
    MAX_COMMAND_BATCH = 32
    MAX_RX_LINE_BYTES = 4096

    line_received = Signal(str)
    connected = Signal(str)
    disconnected = Signal()
    failed = Signal(str)

    def __init__(self, port: str, baud: int, parent=None):
        super().__init__(parent)
        self.port = port
        self.baud = baud
        self._commands: queue.Queue[bytes] = queue.Queue(
            maxsize=self.MAX_PENDING_COMMANDS
        )
        self._close_requested = threading.Event()

    def send(self, command: str) -> None:
        if self._close_requested.is_set():
            raise RuntimeError("serial worker is closing")
        try:
            self._commands.put_nowait(encode_command(command))
        except queue.Full as exc:
            raise RuntimeError("serial command queue is full") from exc

    def shutdown(self, final_commands: Iterable[str] = ()) -> None:
        """Flush final safety commands, then close the port.

        This differs from an immediate thread interruption: queued StopAll/Stream=0
        commands are written before the serial handle is released.
        """
        if self._close_requested.is_set():
            return
        # Shutdown safety commands supersede queued UI commands.
        while True:
            try:
                self._commands.get_nowait()
            except queue.Empty:
                break
        for command in final_commands:
            self._commands.put_nowait(encode_command(command))
        self._close_requested.set()

    def close(self) -> None:
        self.shutdown()

    def _drain_commands(self, ser) -> None:
        for _ in range(self.MAX_COMMAND_BATCH):
            try:
                payload = self._commands.get_nowait()
            except queue.Empty:
                return
            ser.write(payload)

    def run(self) -> None:
        try:
            import serial

            with serial.Serial(
                self.port,
                self.baud,
                timeout=0.05,
                write_timeout=0.5,
            ) as ser:
                ser.reset_input_buffer()
                self.connected.emit(self.port)
                pending = bytearray()

                while not self.isInterruptionRequested():
                    self._drain_commands(ser)
                    if self._close_requested.is_set() and self._commands.empty():
                        ser.flush()
                        break

                    chunk = ser.read(512)
                    if chunk:
                        pending.extend(chunk)
                        while b"\n" in pending:
                            raw, _, pending = pending.partition(b"\n")
                            line = raw.rstrip(b"\r").decode("utf-8", errors="replace")
                            self.line_received.emit(line)
                        if len(pending) > self.MAX_RX_LINE_BYTES:
                            raise RuntimeError(
                                "serial receive line exceeded protocol limit"
                            )
                    else:
                        time.sleep(0.005)
        except Exception as exc:
            self.failed.emit(str(exc))
        finally:
            self.disconnected.emit()
