from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication

from .main_window import MainWindow

APP_STYLE = """
QWidget { font-size: 13px; }
QPushButton, QComboBox, QLineEdit, QSpinBox, QDoubleSpinBox {
    min-height: 30px;
}
QPushButton:focus, QComboBox:focus, QLineEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus, QTabBar::tab:focus {
    border: 2px solid #1976d2;
}
QPushButton:disabled { color: #777; }
"""


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("MSPM0 Configurator")
    app.setStyleSheet(APP_STYLE)
    window = MainWindow()
    window.resize(1250, 820)
    window.show()
    return app.exec()
