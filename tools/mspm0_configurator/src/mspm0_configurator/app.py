from __future__ import annotations

import sys

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QFont
from PySide6.QtWidgets import QApplication

from .main_window import MainWindow
from .theme import FONT_BODY, FONT_FAMILY, build_stylesheet


def apply_native_backdrop(window: MainWindow) -> None:
    """Enable Windows 11 Mica when available; other platforms stay opaque."""
    if sys.platform != "win32":
        return
    try:
        import ctypes

        hwnd = int(window.winId())
        dark = ctypes.c_int(1)
        backdrop = ctypes.c_int(2)  # DWMSBT_MAINWINDOW / Mica
        corners = ctypes.c_int(2)  # DWMWCP_ROUND
        dwm = ctypes.windll.dwmapi.DwmSetWindowAttribute
        dwm(hwnd, 20, ctypes.byref(dark), ctypes.sizeof(dark))
        dwm(hwnd, 38, ctypes.byref(backdrop), ctypes.sizeof(backdrop))
        dwm(hwnd, 33, ctypes.byref(corners), ctypes.sizeof(corners))
    except (AttributeError, OSError, ValueError):
        # Unsupported Windows builds retain the carefully matched opaque theme.
        return


def main() -> int:
    # High DPI rounding policy — must be set before QApplication is created.
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )
    app = QApplication(sys.argv)
    app.setApplicationName("MSPM0 Control Studio")
    app.setStyle("Fusion")

    # Base font in pt — QSS pt values inherit from this.
    base_font = QFont()
    base_font.setFamilies([
        "Segoe UI Variable", "Segoe UI", "Microsoft YaHei UI",
        "Noto Sans CJK SC", "sans-serif",
    ])
    base_font.setPointSize(FONT_BODY)
    app.setFont(base_font)

    app.setStyleSheet(build_stylesheet())
    window = MainWindow()
    window.resize(1360, 860)
    window.show()
    QTimer.singleShot(0, lambda: apply_native_backdrop(window))
    return app.exec()
