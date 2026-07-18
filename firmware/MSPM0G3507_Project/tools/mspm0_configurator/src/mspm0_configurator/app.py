from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication

from .main_window import MainWindow


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("MSPM0 Configurator")
    window = MainWindow()
    window.resize(1250, 820)
    window.show()
    return app.exec()
