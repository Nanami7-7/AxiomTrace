from __future__ import annotations

import html

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QSplitter,
    QTabWidget,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from .i18n import tr
from .plot_widget import TelemetryPlot
from .protocol import (
    CommandBuilder,
    LineKind,
    PLANNER_RPM_LIMIT,
    SUPPORTED_BAUDS,
    decode_line,
)
from .serial_worker import SerialWorker
from .settings import AppSettings


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.settings = AppSettings()
        self.lang = self.settings.language
        self.worker: SerialWorker | None = None
        self.connected_port = ""
        self.machine: dict[str, dict[str, str]] = {}
        self._build_ui()
        self._wire()
        self.refresh_ports()
        self.retranslate()

    def _build_ui(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)

        self.conn_box = QGroupBox()
        connection_row = QHBoxLayout(self.conn_box)
        self.port_label = QLabel()
        self.port = QComboBox()
        self.refresh_btn = QPushButton()
        self.baud_label = QLabel()
        self.baud = QComboBox()
        self.baud.addItems([str(value) for value in SUPPORTED_BAUDS])
        self.baud.setCurrentText("115200")
        self.connect_btn = QPushButton()
        self.language_label = QLabel()
        self.lang_box = QComboBox()
        self.lang_box.addItem("中文", "zh")
        self.lang_box.addItem("English", "en")
        self.status = QLabel()
        self.device = QLabel()
        for widget in (
            self.port_label,
            self.port,
            self.refresh_btn,
            self.baud_label,
            self.baud,
            self.connect_btn,
            self.language_label,
            self.lang_box,
            self.status,
        ):
            connection_row.addWidget(widget)
        connection_row.addStretch(1)
        connection_row.addWidget(self.device)
        outer.addWidget(self.conn_box)

        self.safety = QLabel()
        self.safety.setStyleSheet("color:#b71c1c;font-weight:600;padding:4px")
        outer.addWidget(self.safety)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        outer.addWidget(splitter, 1)
        controls = QTabWidget()
        splitter.addWidget(controls)
        self.plot_tabs = QTabWidget()
        splitter.addWidget(self.plot_tabs)
        splitter.setSizes([430, 800])
        self.control_tabs = controls

        motor_tab = QWidget()
        motor_layout = QVBoxLayout(motor_tab)
        self.motor_box = QGroupBox()
        motor_form = QFormLayout(self.motor_box)
        self.motor_label = QLabel()
        self.motor = QComboBox()
        self.motor.addItems([
            "A / M1 / RB",
            "B / M2 / RF",
            "C / M3 / LF",
            "D / M4 / LB",
        ])
        motor_form.addRow(self.motor_label, self.motor)
        self.kp = self._spin(-100, 100, 0.8, 4)
        self.ki = self._spin(-100, 100, 0.3, 4)
        self.kd = self._spin(-100, 100, 0.0, 4)
        self.target = self._spin(-800, 800, 0, 1)
        self.kp_label = QLabel("Kp")
        self.ki_label = QLabel("Ki")
        self.kd_label = QLabel("Kd")
        self.target_label = QLabel()
        motor_form.addRow(self.kp_label, self.kp)
        motor_form.addRow(self.ki_label, self.ki)
        motor_form.addRow(self.kd_label, self.kd)
        motor_form.addRow(self.target_label, self.target)
        motor_buttons = QHBoxLayout()
        self.apply_btn = QPushButton()
        self.run_btn = QPushButton()
        self.stop_btn = QPushButton()
        self.stop_all_btn = QPushButton()
        self.stop_all_btn.setStyleSheet(
            "background:#b71c1c;color:white;font-weight:bold"
        )
        for widget in (
            self.apply_btn,
            self.run_btn,
            self.stop_btn,
            self.stop_all_btn,
        ):
            motor_buttons.addWidget(widget)
        motor_form.addRow(motor_buttons)
        motor_layout.addWidget(self.motor_box)

        self.ff_box = QGroupBox()
        ff_form = QFormLayout(self.ff_box)
        self.ff_k = self._spin(-10_000, 10_000, 0, 6)
        self.ff_b = self._spin(-10_000, 10_000, 0, 6)
        self.ff_enable = QCheckBox()
        self.ff_apply = QPushButton()
        ff_form.addRow("k", self.ff_k)
        ff_form.addRow("b", self.ff_b)
        ff_form.addRow(self.ff_enable)
        ff_form.addRow(self.ff_apply)
        motor_layout.addWidget(self.ff_box)
        motor_layout.addStretch(1)
        controls.addTab(motor_tab, "")

        motion_tab = QWidget()
        motion_form = QFormLayout(motion_tab)
        self.mode = QComboBox()
        self.mode.addItem("Speed", "speed")
        self.mode.addItem("Position", "position")
        self.mode.addItem("Angle", "angle")
        self.pulses = self._spin(-1e9, 1e9, 1000, 0)
        self.degrees = self._spin(-36_000, 36_000, 90, 1)
        self.cruise = self._spin(0.1, PLANNER_RPM_LIMIT, 100, 1)
        self.motion_start = QPushButton()
        self.abort_btn = QPushButton()
        self.mode_label = QLabel()
        self.pulses_label = QLabel()
        self.degrees_label = QLabel()
        self.cruise_label = QLabel()
        motion_form.addRow(self.mode_label, self.mode)
        motion_form.addRow(self.pulses_label, self.pulses)
        motion_form.addRow(self.degrees_label, self.degrees)
        motion_form.addRow(self.cruise_label, self.cruise)
        motion_form.addRow(self.motion_start, self.abort_btn)
        controls.addTab(motion_tab, "")

        config_tab = QWidget()
        config_layout = QVBoxLayout(config_tab)
        self.info_text = QTextEdit()
        self.info_text.setReadOnly(True)
        self.query_btn = QPushButton()
        self.save_btn = QPushButton()
        self.load_btn = QPushButton()
        config_layout.addWidget(self.info_text)
        config_layout.addWidget(self.query_btn)
        config_layout.addWidget(self.save_btn)
        config_layout.addWidget(self.load_btn)
        controls.addTab(config_tab, "")

        plot_page = QWidget()
        plot_layout = QVBoxLayout(plot_page)
        plot_toolbar = QHBoxLayout()
        self.stream_on = QPushButton()
        self.stream_off = QPushButton()
        self.pause = QPushButton()
        self.pause.setCheckable(True)
        self.clear_btn = QPushButton()
        self.export_btn = QPushButton()
        self.window_label = QLabel()
        self.window = QSpinBox()
        self.window.setRange(3, 300)
        self.window.setValue(15)
        for widget in (
            self.stream_on,
            self.stream_off,
            self.pause,
            self.clear_btn,
            self.export_btn,
            self.window_label,
            self.window,
        ):
            plot_toolbar.addWidget(widget)
        plot_toolbar.addStretch(1)
        plot_layout.addLayout(plot_toolbar)
        self.plot = TelemetryPlot()
        plot_layout.addWidget(self.plot, 1)
        self.plot_tabs.addTab(plot_page, "")

        console_page = QWidget()
        console_layout = QVBoxLayout(console_page)
        self.console = QTextEdit()
        self.console.setReadOnly(True)
        send_row = QHBoxLayout()
        self.command = QLineEdit()
        self.send_btn = QPushButton()
        send_row.addWidget(self.command, 1)
        send_row.addWidget(self.send_btn)
        console_layout.addWidget(self.console, 1)
        console_layout.addLayout(send_row)
        self.plot_tabs.addTab(console_page, "")

    @staticmethod
    def _spin(low: float, high: float, value: float, decimals: int) -> QDoubleSpinBox:
        widget = QDoubleSpinBox()
        widget.setRange(low, high)
        widget.setDecimals(decimals)
        widget.setValue(value)
        widget.setKeyboardTracking(False)
        return widget

    def _wire(self) -> None:
        self.refresh_btn.clicked.connect(self.refresh_ports)
        self.connect_btn.clicked.connect(self.toggle_connection)
        self.lang_box.currentIndexChanged.connect(self.change_language)
        self.apply_btn.clicked.connect(self.apply_control)
        self.run_btn.clicked.connect(self.run_motor)
        self.stop_btn.clicked.connect(
            lambda: self.send(CommandBuilder.stop(self.motor.currentIndex()))
        )
        self.stop_all_btn.clicked.connect(lambda: self.send(CommandBuilder.stop_all()))
        self.ff_apply.clicked.connect(self.apply_ff)
        self.motion_start.clicked.connect(self.start_motion)
        self.abort_btn.clicked.connect(lambda: self.send(CommandBuilder.abort()))
        self.stream_on.clicked.connect(lambda: self.send(CommandBuilder.stream(True)))
        self.stream_off.clicked.connect(lambda: self.send(CommandBuilder.stream(False)))
        self.pause.toggled.connect(self._pause)
        self.clear_btn.clicked.connect(self.plot.clear)
        self.export_btn.clicked.connect(self.export_csv)
        self.window.valueChanged.connect(self.plot.set_window)
        self.send_btn.clicked.connect(self.send_console)
        self.command.returnPressed.connect(self.send_console)
        self.query_btn.clicked.connect(self.query_device)
        self.save_btn.clicked.connect(self.save_config)
        self.load_btn.clicked.connect(self.load_config)

    def refresh_ports(self) -> None:
        selected_port = self.port.currentData()
        self.port.clear()
        try:
            from serial.tools import list_ports

            ports = sorted(list_ports.comports(), key=lambda item: item.device)
            for item in ports:
                self.port.addItem(f"{item.device} — {item.description}", item.device)
        except Exception as exc:
            self.console.append(html.escape(f"Serial scan failed: {exc}"))
        if selected_port:
            index = self.port.findData(selected_port)
            if index >= 0:
                self.port.setCurrentIndex(index)

    def toggle_connection(self) -> None:
        if self.worker is not None:
            self.disconnect_serial()
            return
        port = self.port.currentData()
        if not port:
            return
        worker = SerialWorker(port, int(self.baud.currentText()), self)
        worker.line_received.connect(self.on_line)
        worker.connected.connect(self.on_connected)
        worker.disconnected.connect(self.on_disconnected)
        worker.failed.connect(self.on_error)
        self.worker = worker
        self.retranslate()
        worker.start()

    def disconnect_serial(self) -> None:
        worker = self.worker
        if worker is None:
            return
        self.worker = None
        worker.shutdown((CommandBuilder.stop_all(), CommandBuilder.stream(False)))
        if not worker.wait(1500):
            worker.requestInterruption()
            worker.wait(500)
        worker.deleteLater()
        self.connected_port = ""
        self.retranslate()

    def on_connected(self, port: str) -> None:
        self.connected_port = port
        self.retranslate()
        self.query_device()

    def on_disconnected(self) -> None:
        self.worker = None
        self.connected_port = ""
        self.retranslate()

    def on_error(self, message: str) -> None:
        QMessageBox.critical(self, tr(self.lang, "error"), message)

    def send(self, command: str) -> None:
        worker = self.worker
        if worker is None:
            return
        try:
            worker.send(command)
            self.console.append(f"<b>TX:</b> {html.escape(command)}")
        except Exception as exc:
            self.on_error(str(exc))

    def send_console(self) -> None:
        text = self.command.text().strip()
        if text:
            self.send(text)
            self.command.clear()

    def on_line(self, line: str) -> None:
        decoded = decode_line(line)
        if decoded.kind is LineKind.TELEMETRY:
            assert decoded.telemetry is not None
            self.plot.add_frame(decoded.telemetry)
            return

        self.console.append(html.escape(line))
        if decoded.kind is LineKind.MACHINE:
            assert decoded.machine is not None
            self.machine[decoded.machine.topic] = dict(decoded.machine.fields)
            rows = []
            for topic, fields in self.machine.items():
                rows.append(
                    f"@{topic}: " + ", ".join(f"{key}={value}" for key, value in fields.items())
                )
            self.info_text.setPlainText("\n".join(rows))
            if decoded.machine.topic == "INFO":
                firmware = decoded.machine.fields.get("fw", "?")
                protocol = decoded.machine.fields.get("proto", "?")
                self.device.setText(f"FW {firmware} / Proto {protocol}")

    def query_device(self) -> None:
        for command in (
            CommandBuilder.info_query(),
            CommandBuilder.config_query(),
            CommandBuilder.status(self.motor.currentIndex()),
        ):
            self.send(command)

    def apply_control(self) -> None:
        motor_id = self.motor.currentIndex()
        self.send(CommandBuilder.motor(motor_id))
        for name, widget in (("kp", self.kp), ("ki", self.ki), ("kd", self.kd)):
            self.send(CommandBuilder.pid(name, widget.value()))
        self.send(CommandBuilder.target(self.target.value()))
        self.send(CommandBuilder.status(motor_id))

    def run_motor(self) -> None:
        motor_id = self.motor.currentIndex()
        motor_name = "ABCD"[motor_id]
        answer = QMessageBox.question(
            self,
            tr(self.lang, "run"),
            tr(self.lang, "confirm_run").format(
                motor=motor_name,
                rpm=self.target.value(),
            ),
        )
        if answer == QMessageBox.StandardButton.Yes:
            self.apply_control()
            self.send(CommandBuilder.run(motor_id))

    def apply_ff(self) -> None:
        self.send(CommandBuilder.motor(self.motor.currentIndex()))
        commands = CommandBuilder.feedforward(
            self.ff_k.value(),
            self.ff_b.value(),
            self.ff_enable.isChecked(),
        )
        for command in commands:
            self.send(command)

    def start_motion(self) -> None:
        mode = self.mode.currentData()
        self.send(CommandBuilder.mode(mode))
        if mode == "position":
            self.send(CommandBuilder.position(self.pulses.value(), self.cruise.value()))
        elif mode == "angle":
            self.send(CommandBuilder.angle(self.degrees.value(), self.cruise.value()))

    def _pause(self, value: bool) -> None:
        self.plot.paused = value

    def export_csv(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self,
            tr(self.lang, "export"),
            "telemetry.csv",
            "CSV (*.csv)",
        )
        if path:
            self.plot.export_csv(path)

    def collect_settings(self) -> AppSettings:
        return AppSettings(
            language=self.lang,
            baud=int(self.baud.currentText()),
            motor=self.motor.currentIndex(),
            kp=self.kp.value(),
            ki=self.ki.value(),
            kd=self.kd.value(),
            target_rpm=self.target.value(),
            ff_k=self.ff_k.value(),
            ff_b=self.ff_b.value(),
            ff_enabled=self.ff_enable.isChecked(),
            plot_window_s=self.window.value(),
        )

    def save_config(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self,
            tr(self.lang, "save"),
            "mspm0-config.json",
            "JSON (*.json)",
        )
        if not path:
            return
        try:
            self.collect_settings().save(path)
        except Exception as exc:
            self.on_error(str(exc))

    def load_config(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self,
            tr(self.lang, "load"),
            "",
            "JSON (*.json)",
        )
        if not path:
            return
        try:
            settings = AppSettings.load(path)
        except Exception as exc:
            self.on_error(str(exc))
            return

        self.settings = settings
        self.lang = settings.language
        self.lang_box.setCurrentIndex(max(0, self.lang_box.findData(settings.language)))
        self.baud.setCurrentText(str(settings.baud))
        self.motor.setCurrentIndex(settings.motor)
        self.kp.setValue(settings.kp)
        self.ki.setValue(settings.ki)
        self.kd.setValue(settings.kd)
        self.target.setValue(settings.target_rpm)
        self.ff_k.setValue(settings.ff_k)
        self.ff_b.setValue(settings.ff_b)
        self.ff_enable.setChecked(settings.ff_enabled)
        self.window.setValue(settings.plot_window_s)
        self.retranslate()

    def change_language(self, _index: int = -1) -> None:
        self.lang = self.lang_box.currentData() or "en"
        self.retranslate()

    def retranslate(self) -> None:
        self.setWindowTitle(tr(self.lang, "title"))
        self.conn_box.setTitle(tr(self.lang, "connection"))
        self.port_label.setText(tr(self.lang, "port"))
        self.refresh_btn.setText(tr(self.lang, "refresh"))
        self.baud_label.setText(tr(self.lang, "baud"))
        self.connect_btn.setText(
            tr(self.lang, "disconnect") if self.worker else tr(self.lang, "connect")
        )
        self.language_label.setText(tr(self.lang, "language"))
        if self.connected_port:
            self.status.setText(f"{tr(self.lang, 'connected')}: {self.connected_port}")
        else:
            self.status.setText(tr(self.lang, "disconnected"))
        self.safety.setText(tr(self.lang, "safety"))
        self.motor_box.setTitle(tr(self.lang, "pid"))
        self.motor_label.setText(tr(self.lang, "motor"))
        self.target_label.setText(tr(self.lang, "target"))
        self.apply_btn.setText(tr(self.lang, "apply"))
        self.run_btn.setText(tr(self.lang, "run"))
        self.stop_btn.setText(tr(self.lang, "stop"))
        self.stop_all_btn.setText(tr(self.lang, "stop_all"))
        self.ff_box.setTitle(tr(self.lang, "ff"))
        self.ff_enable.setText(tr(self.lang, "enabled"))
        self.ff_apply.setText(tr(self.lang, "apply"))
        self.control_tabs.setTabText(0, tr(self.lang, "motor"))
        self.control_tabs.setTabText(1, tr(self.lang, "motion"))
        self.control_tabs.setTabText(2, tr(self.lang, "device"))
        self.mode_label.setText(tr(self.lang, "mode"))
        for index, key in enumerate(("speed", "position", "angle")):
            self.mode.setItemText(index, tr(self.lang, key))
        self.pulses_label.setText(tr(self.lang, "pulses"))
        self.degrees_label.setText(tr(self.lang, "degrees"))
        self.cruise_label.setText(tr(self.lang, "cruise"))
        self.motion_start.setText(tr(self.lang, "start"))
        self.abort_btn.setText(tr(self.lang, "abort"))
        self.query_btn.setText(tr(self.lang, "query"))
        self.stream_on.setText(tr(self.lang, "stream_on"))
        self.stream_off.setText(tr(self.lang, "stream_off"))
        self.pause.setText(tr(self.lang, "pause"))
        self.clear_btn.setText(tr(self.lang, "clear"))
        self.export_btn.setText(tr(self.lang, "export"))
        self.window_label.setText(tr(self.lang, "seconds"))
        self.save_btn.setText(tr(self.lang, "save"))
        self.load_btn.setText(tr(self.lang, "load"))
        self.send_btn.setText(tr(self.lang, "send"))
        self.plot_tabs.setTabText(0, tr(self.lang, "telemetry"))
        self.plot_tabs.setTabText(1, tr(self.lang, "console"))
        self._update_connection_controls()

    def _update_connection_controls(self) -> None:
        """Keep device commands unavailable until the port is connected."""
        connected = bool(self.connected_port)
        for widget in (
            self.apply_btn,
            self.run_btn,
            self.stop_btn,
            self.stop_all_btn,
            self.ff_apply,
            self.motion_start,
            self.abort_btn,
            self.query_btn,
            self.stream_on,
            self.stream_off,
            self.command,
            self.send_btn,
        ):
            widget.setEnabled(connected)

    def closeEvent(self, event) -> None:
        self.disconnect_serial()
        event.accept()
