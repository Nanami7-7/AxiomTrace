from __future__ import annotations

import html
import time

from PySide6.QtCore import QEasingCurve, QPointF, QPropertyAnimation, QTimer, Qt
from PySide6.QtGui import QKeySequence, QShortcut
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QSplitter,
    QTabWidget,
    QTableWidget,
    QTableWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from .i18n import tr
from .plot_widget import TelemetryPlot
from .protocol import (
    CHANNEL_NAMES,
    MOTOR_COUNT,
    PROTOCOL_VERSION,
    CommandBuilder,
    LineKind,
    MotorStatus,
    PLANNER_RPM_LIMIT,
    SUPPORTED_BAUDS,
    decode_line,
    parse_device_info,
    parse_motor_status,
)
from .serial_worker import SerialWorker
from .settings import AppSettings
from .icons import icon as svg_icon, pixmap as svg_pixmap
from .theme import Icons, Palette, RADIUS_CONTAINER, SPACING_LG, SPACING_MD, SPACING_SM, SPACING_XL, SPACING_XS


class MainWindow(QMainWindow):
    HANDSHAKE_TIMEOUT_MS = 2500

    # ── Motor display names ──────────────────────────────────────────
    MOTOR_LABELS = ("motor_a_name", "motor_b_name", "motor_c_name", "motor_d_name")

    # ── Quick console commands ───────────────────────────────────────
    QUICK_COMMANDS = ("Info?", "Config?", "Status?", "Stream=0", "Stream=1")

    def __init__(self):
        super().__init__()
        self.settings = AppSettings()
        self.lang = self.settings.language
        self.worker: SerialWorker | None = None
        self.connected_port = ""
        self.connection_state = "disconnected"
        self.handshake_ok = False
        self.streaming = False
        self.machine: dict[str, dict[str, str]] = {}
        self.motor_statuses: dict[int, MotorStatus] = {}
        self.device_info = None
        self._disconnecting = False
        self._focus_mode = False
        self._window_was_maximized = False
        self._pre_focus_left_visible = True
        self._pre_focus_right_visible = True
        self._main_splitter_sizes = list(self.settings.main_splitter_sizes)
        self._telemetry_splitter_sizes = list(self.settings.telemetry_splitter_sizes)
        self._sidebar_animations: dict[QWidget, QPropertyAnimation] = {}
        self._input_rate = 0.0
        self._render_rate = 0.0
        self._connect_start_time: float | None = None

        # Console state
        self._console_history: list[str] = []
        self._console_history_index: int = -1
        self._autoscroll: bool = True
        self._rx_bytes: int = 0
        self._tx_bytes: int = 0
        self._has_unsaved_changes: bool = False

        # Emergency stop via shortcut tracking
        self._emergency_from_shortcut: bool = False

        self.handshake_timer = QTimer(self)
        self.handshake_timer.setSingleShot(True)
        self.handshake_timer.timeout.connect(self._handshake_timeout)

        # Pulse animation timer for connecting / emergency button
        self._pulse_timer = QTimer(self)
        self._pulse_timer.setInterval(600)
        self._pulse_step = 0
        self._pulse_timer.timeout.connect(self._on_pulse_tick)

        # Connection duration timer
        self._duration_timer = QTimer(self)
        self._duration_timer.setInterval(1000)
        self._duration_timer.timeout.connect(self._update_duration)

        self._build_ui()
        self._wire()
        self._restore_geometry()
        self.refresh_ports()
        self.retranslate()

    # ════════════════════════════════════════════════════════════════
    #  UI Construction
    # ════════════════════════════════════════════════════════════════

    def _build_ui(self) -> None:
        self.setMinimumSize(980, 640)
        root = QWidget()
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)
        outer.setContentsMargins(16, 14, 16, 12)
        outer.setSpacing(10)

        # ── Header ───────────────────────────────────────────────────
        self.header = QFrame()
        self.header.setObjectName("headerPanel")
        header_layout = QHBoxLayout(self.header)
        header_layout.setContentsMargins(16, 12, 12, 12)
        title_block = QVBoxLayout()
        self.brand = QLabel("MSPM0 Control Studio")
        self.brand.setObjectName("brandTitle")
        self.subtitle = QLabel("G3507 · 4-axis motor engineering console")
        self.subtitle.setObjectName("mutedText")
        title_block.addWidget(self.brand)
        title_block.addWidget(self.subtitle)
        header_layout.addLayout(title_block)
        header_layout.addStretch(1)
        self.device = QLabel("—")
        self.device.setObjectName("deviceBadge")
        self.status = QLabel()
        self.status.setObjectName("stateBadge")
        self.emergency_btn = QPushButton()
        self.emergency_btn.setObjectName("emergencyButton")
        self.emergency_btn.setMinimumHeight(48)
        self.emergency_btn.setAccessibleName("Emergency stop all motors")
        header_layout.addWidget(self.device)
        header_layout.addWidget(self.status)
        header_layout.addWidget(self.emergency_btn)
        outer.addWidget(self.header)

        # ── Connection bar ───────────────────────────────────────────
        self.conn_box = QGroupBox()
        connection_row = QHBoxLayout(self.conn_box)
        connection_row.setSpacing(8)
        self.port_label = QLabel()
        self.port = QComboBox()
        self.port.setMinimumWidth(210)
        self.refresh_btn = QPushButton()
        self.baud_label = QLabel()
        self.baud = QComboBox()
        self.baud.addItems([str(value) for value in SUPPORTED_BAUDS])
        self.baud.setCurrentText("115200")
        self.connect_btn = QPushButton()
        self.connect_btn.setObjectName("connectButton")
        # DTR/RTS indicator dots
        self.dtr_dot = QLabel()
        self.dtr_dot.setObjectName("dtrDot")
        self.rts_dot = QLabel()
        self.rts_dot.setObjectName("rtsDot")
        self.dtr_dot.setVisible(False)
        self.rts_dot.setVisible(False)
        self.language_label = QLabel()
        self.lang_box = QComboBox()
        self.lang_box.addItem("中文", "zh")
        self.lang_box.addItem("English", "en")
        for widget in (
            self.port_label, self.port, self.refresh_btn, self.baud_label,
            self.baud, self.connect_btn, self.dtr_dot, self.rts_dot,
        ):
            connection_row.addWidget(widget)
        connection_row.addStretch(1)
        connection_row.addWidget(self.language_label)
        connection_row.addWidget(self.lang_box)
        outer.addWidget(self.conn_box)

        # ── Safety banner ────────────────────────────────────────────
        self.safety = QLabel()
        self.safety.setObjectName("safetyBanner")
        self.safety.setWordWrap(True)
        outer.addWidget(self.safety)

        # ── Workspace: left rail + control tabs + plot tabs ──────────
        self.workspace_container = QWidget()
        ws_layout = QHBoxLayout(self.workspace_container)
        ws_layout.setContentsMargins(0, 0, 0, 0)
        ws_layout.setSpacing(4)

        # Left collapsed rail (visible when sidebar is hidden)
        self.left_rail = QFrame()
        self.left_rail.setObjectName("leftRail")
        rail_layout = QVBoxLayout(self.left_rail)
        rail_layout.setContentsMargins(4, 8, 4, 8)
        rail_layout.setSpacing(4)
        self.rail_motor_btn = QPushButton()
        self.rail_motor_btn.setObjectName("iconButton")
        self.rail_motor_btn.setIcon(svg_icon("motor", size=18))
        self.rail_motion_btn = QPushButton()
        self.rail_motion_btn.setObjectName("iconButton")
        self.rail_motion_btn.setIcon(svg_icon("motion", size=18))
        self.rail_device_btn = QPushButton()
        self.rail_device_btn.setObjectName("iconButton")
        self.rail_device_btn.setIcon(svg_icon("device", size=18))
        self.rail_expand_btn = QPushButton()
        self.rail_expand_btn.setObjectName("iconButton")
        self.rail_expand_btn.setIcon(svg_icon("expand_left", size=18))
        for btn in (self.rail_motor_btn, self.rail_motion_btn, self.rail_device_btn):
            rail_layout.addWidget(btn)
        rail_layout.addStretch(1)
        rail_layout.addWidget(self.rail_expand_btn)
        self.left_rail.setVisible(False)
        ws_layout.addWidget(self.left_rail)

        # Main splitter (control tabs | plot tabs)
        self.workspace_splitter = QSplitter(Qt.Orientation.Horizontal)
        self.workspace_splitter.setChildrenCollapsible(False)
        ws_layout.addWidget(self.workspace_splitter)

        self.control_tabs = QTabWidget()
        self.control_tabs.setMinimumWidth(360)
        self.workspace_splitter.addWidget(self.control_tabs)
        self.plot_tabs = QTabWidget()
        self.workspace_splitter.addWidget(self.plot_tabs)
        self.workspace_splitter.setStretchFactor(0, 0)
        self.workspace_splitter.setStretchFactor(1, 1)
        self.workspace_splitter.setSizes(self._main_splitter_sizes)

        # Right collapsed rail (visible when values sidebar hidden)
        self.right_rail = QFrame()
        self.right_rail.setObjectName("rightRail")
        right_rail_layout = QVBoxLayout(self.right_rail)
        right_rail_layout.setContentsMargins(6, 8, 6, 8)
        right_rail_layout.setSpacing(4)
        self.right_rail_rpm_label = QLabel("—")
        self.right_rail_rpm_label.setObjectName("largeValue")
        self.right_rail_rpm_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.right_rail_rpm_caption = QLabel("RPM")
        self.right_rail_rpm_caption.setObjectName("mutedText")
        self.right_rail_rpm_caption.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.right_rail_state_label = QLabel("—")
        self.right_rail_state_label.setObjectName("stateBadge")
        self.right_rail_state_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.right_rail_expand_btn = QPushButton()
        self.right_rail_expand_btn.setObjectName("iconButton")
        self.right_rail_expand_btn.setIcon(svg_icon("expand_right", size=18))
        right_rail_layout.addWidget(self.right_rail_rpm_caption)
        right_rail_layout.addWidget(self.right_rail_rpm_label)
        right_rail_layout.addWidget(self.right_rail_state_label)
        right_rail_layout.addStretch(1)
        right_rail_layout.addWidget(self.right_rail_expand_btn)
        self.right_rail.setVisible(False)
        ws_layout.addWidget(self.right_rail)

        outer.addWidget(self.workspace_container, 1)

        # Build tabs
        self._build_motor_tab()
        self._build_motion_tab()
        self._build_device_tab()
        self._build_telemetry_tab()
        self._build_console_tab()

        # ── Status bar ───────────────────────────────────────────────
        self.statusBar().setSizeGripEnabled(False)
        self.statusBar().showMessage("MSPM0 Control Studio")

        # Motor indicator in status bar
        self.motor_indicator = QLabel()
        self.statusBar().addPermanentWidget(self.motor_indicator)

        # Port info in status bar
        self.port_info_label = QLabel()
        self.statusBar().addPermanentWidget(self.port_info_label)

        # Duration label in status bar
        self.duration_label = QLabel()
        self.statusBar().addPermanentWidget(self.duration_label)

    def _build_motor_tab(self) -> None:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setContentsMargins(12, 14, 12, 12)
        layout.setSpacing(12)

        self.motor_state_box = QGroupBox()
        self.motor_state_box.setObjectName("motorStateBox")
        self.motor_state_box.setMinimumHeight(160)
        state_grid = QGridLayout(self.motor_state_box)
        # State labels with prefix captions for clarity
        self.motor_state = QLabel("—")
        self.motor_state.setObjectName("largeValue")
        self.motor_rpm = QLabel("— RPM")
        self.motor_rpm.setObjectName("largeValue")
        self.motor_output = QLabel("—")
        self.motor_mode = QLabel("—")
        self.motor_deviation = QLabel("—")
        self.motor_deviation.setObjectName("mutedText")
        # Captions above values for structured grid layout
        self._motor_state_caption = QLabel()
        self._motor_state_caption.setObjectName("mutedText")
        self._motor_rpm_caption = QLabel()
        self._motor_rpm_caption.setObjectName("mutedText")
        self._motor_output_caption = QLabel()
        self._motor_output_caption.setObjectName("mutedText")
        self._motor_mode_caption = QLabel()
        self._motor_mode_caption.setObjectName("mutedText")
        state_grid.addWidget(self._motor_state_caption, 0, 0)
        state_grid.addWidget(self._motor_rpm_caption, 0, 1)
        state_grid.addWidget(self.motor_state, 1, 0)
        state_grid.addWidget(self.motor_rpm, 1, 1)
        state_grid.addWidget(self._motor_output_caption, 2, 0)
        state_grid.addWidget(self._motor_mode_caption, 2, 1)
        state_grid.addWidget(self.motor_output, 3, 0)
        state_grid.addWidget(self.motor_mode, 3, 1)
        state_grid.addWidget(self.motor_deviation, 4, 0, 1, 2)
        layout.addWidget(self.motor_state_box)

        self.motor_box = QGroupBox()
        self.motor_box.setMinimumHeight(300)
        motor_form = QFormLayout(self.motor_box)
        self.motor_label = QLabel()
        self.motor = QComboBox()
        self.motor.addItems((
            "A / M1 / RB", "B / M2 / RF", "C / M3 / LF", "D / M4 / LB",
        ))
        motor_form.addRow(self.motor_label, self.motor)
        self.kp = self._spin(-100, 100, 0.8, 4)
        self.ki = self._spin(-100, 100, 0.3, 4)
        self.kd = self._spin(-100, 100, 0.0, 4)
        self.target = self._spin(-800, 800, 0, 1)
        self.target.setSuffix(" RPM")
        self.kp_label = QLabel("Kp")
        self.ki_label = QLabel("Ki")
        self.kd_label = QLabel("Kd")
        self.kp_label.setBuddy(self.kp)
        self.ki_label.setBuddy(self.ki)
        self.kd_label.setBuddy(self.kd)
        motor_form.addRow(self.kp_label, self.kp)
        motor_form.addRow(self.ki_label, self.ki)
        motor_form.addRow(self.kd_label, self.kd)
        self.target_label = QLabel()
        self.target_label.setBuddy(self.target)
        motor_form.addRow(self.target_label, self.target)
        motor_buttons = QHBoxLayout()
        self.apply_btn = QPushButton()
        self.run_btn = QPushButton()
        self.run_btn.setObjectName("primaryButton")
        self.stop_btn = QPushButton()
        self.stop_btn.setObjectName("warningButton")
        for widget in (self.apply_btn, self.run_btn, self.stop_btn):
            motor_buttons.addWidget(widget)
        motor_form.addRow(motor_buttons)
        layout.addWidget(self.motor_box)

        self.ff_box = QGroupBox()
        self.ff_box.setMinimumHeight(190)
        ff_form = QFormLayout(self.ff_box)
        self.ff_k = self._spin(-10_000, 10_000, 0, 6)
        self.ff_b = self._spin(-10_000, 10_000, 0, 6)
        self.ff_enable = QCheckBox()
        self.ff_apply = QPushButton()
        self.ff_k_label = QLabel("k")
        self.ff_b_label = QLabel("b")
        self.ff_k_label.setBuddy(self.ff_k)
        self.ff_b_label.setBuddy(self.ff_b)
        ff_form.addRow(self.ff_k_label, self.ff_k)
        ff_form.addRow(self.ff_b_label, self.ff_b)
        ff_form.addRow(self.ff_enable)
        ff_form.addRow(self.ff_apply)
        layout.addWidget(self.ff_box)
        layout.addStretch(1)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setWidget(tab)
        self.control_tabs.addTab(scroll, "")

    def _build_motion_tab(self) -> None:
        tab = QWidget()
        form = QFormLayout(tab)
        form.setContentsMargins(12, 16, 12, 12)
        self.mode = QComboBox()
        self.mode.addItem("Speed", "speed")
        self.mode.addItem("Position", "position")
        self.mode.addItem("Angle", "angle")
        self.pulses = self._spin(-1e9, 1e9, 1000, 0)
        self.degrees = self._spin(-36_000, 36_000, 90, 1)
        self.degrees.setSuffix("\u00B0")
        self.cruise = self._spin(0.1, PLANNER_RPM_LIMIT, 100, 1)
        self.cruise.setSuffix(" RPM")
        self.motion_start = QPushButton()
        self.motion_start.setObjectName("primaryButton")
        self.abort_btn = QPushButton()
        self.abort_btn.setObjectName("warningButton")
        self.mode_label = QLabel()
        self.mode_label.setBuddy(self.mode)
        self.pulses_label = QLabel()
        self.pulses_label.setBuddy(self.pulses)
        self.degrees_label = QLabel()
        self.degrees_label.setBuddy(self.degrees)
        self.cruise_label = QLabel()
        self.cruise_label.setBuddy(self.cruise)
        form.addRow(self.mode_label, self.mode)
        form.addRow(self.pulses_label, self.pulses)
        form.addRow(self.degrees_label, self.degrees)
        form.addRow(self.cruise_label, self.cruise)
        buttons = QHBoxLayout()
        buttons.addWidget(self.motion_start)
        buttons.addWidget(self.abort_btn)
        form.addRow(buttons)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setWidget(tab)
        self.control_tabs.addTab(scroll, "")

    def _build_device_tab(self) -> None:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setContentsMargins(12, 14, 12, 12)
        self.device_summary_label = QLabel()
        self.device_summary_label.setObjectName("sectionTitle")
        self.device_summary = QLabel("—")
        self.device_summary.setWordWrap(True)
        self.raw_protocol_label = QLabel()
        self.raw_protocol_label.setObjectName("sectionTitle")
        self.info_text = QTextEdit()
        self.info_text.setReadOnly(True)
        self.info_text.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        buttons = QHBoxLayout()
        self.query_btn = QPushButton()
        self.save_btn = QPushButton()
        self.load_btn = QPushButton()
        for widget in (self.query_btn, self.save_btn, self.load_btn):
            buttons.addWidget(widget)
        layout.addWidget(self.device_summary_label)
        layout.addWidget(self.device_summary)
        layout.addWidget(self.raw_protocol_label)
        layout.addWidget(self.info_text, 1)
        layout.addLayout(buttons)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setWidget(tab)
        self.control_tabs.addTab(scroll, "")

    def _build_telemetry_tab(self) -> None:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(10, 10, 10, 10)
        action_toolbar = QHBoxLayout()

        # Compact icon-style buttons
        self.stream_on = QPushButton()
        self.stream_on.setObjectName("primaryButton")
        self.stream_on.setIcon(svg_icon("stream_on", size=16))
        self.stream_off = QPushButton()
        self.stream_off.setIcon(svg_icon("stream_off", size=16))
        self.pause = QPushButton()
        self.pause.setCheckable(True)
        self.pause.setIcon(svg_icon("pause", size=16))
        self.clear_btn = QPushButton()
        self.clear_btn.setIcon(svg_icon("clear", size=16))
        self.export_btn = QPushButton()
        self.export_btn.setIcon(svg_icon("export", size=16))
        self.focus_btn = QPushButton()
        self.focus_btn.setIcon(svg_icon("focus", size=16))
        self.focus_btn.setCheckable(True)

        for btn in (self.stream_on, self.stream_off, self.pause, self.clear_btn,
                     self.export_btn, self.focus_btn):
            btn.setObjectName("iconButton")

        # Separator
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.VLine)
        sep.setFixedWidth(2)

        self.window_label = QLabel()
        self.window = QSpinBox()
        self.window.setRange(3, 300)
        self.window.setValue(15)
        self.window_label.setBuddy(self.window)

        # Series checkboxes with colour indicators
        self.rpm_series = QCheckBox()
        self.rpm_series.setChecked(True)
        self.target_series = QCheckBox()
        self.target_series.setChecked(True)
        self.control_series = QCheckBox()
        self.sample_rate = QLabel("0.0 Hz")
        self.sample_rate.setObjectName("deviceBadge")

        for widget in (self.stream_on, self.stream_off, self.pause, self.clear_btn,
                        self.export_btn):
            action_toolbar.addWidget(widget)
        action_toolbar.addWidget(sep)
        action_toolbar.addWidget(self.focus_btn)
        action_toolbar.addStretch(1)
        action_toolbar.addWidget(self.sample_rate)

        options_toolbar = QHBoxLayout()
        for widget in (
            self.window_label, self.window, self.rpm_series,
            self.target_series, self.control_series,
        ):
            options_toolbar.addWidget(widget)
        options_toolbar.addStretch(1)
        layout.addLayout(action_toolbar)
        layout.addLayout(options_toolbar)

        # Telemetry splitter (plot | right rail summary / live table)
        self.telemetry_splitter = QSplitter(Qt.Orientation.Horizontal)
        self.telemetry_splitter.setChildrenCollapsible(False)
        self.plot = TelemetryPlot()
        self.telemetry_splitter.addWidget(self.plot)
        self.live_table = QTableWidget(len(CHANNEL_NAMES), 2)
        self.live_table.setHorizontalHeaderLabels(("Channel", "Value"))
        self.live_table.verticalHeader().setVisible(False)
        self.live_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.live_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.live_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.live_table.setAlternatingRowColors(True)
        for row, name in enumerate(CHANNEL_NAMES):
            self.live_table.setItem(row, 0, QTableWidgetItem(name))
            self.live_table.setItem(row, 1, QTableWidgetItem("—"))
        self.telemetry_splitter.addWidget(self.live_table)
        self.telemetry_splitter.setStretchFactor(0, 1)
        self.telemetry_splitter.setSizes(self._telemetry_splitter_sizes)
        layout.addWidget(self.telemetry_splitter, 1)
        self.plot_tabs.addTab(page, "")

    def _build_console_tab(self) -> None:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(10, 10, 10, 10)

        # Quick command buttons row
        quick_row = QHBoxLayout()
        quick_label = QLabel()
        quick_label.setObjectName("mutedText")
        quick_row.addWidget(quick_label)
        self._quick_cmd_buttons: list[QPushButton] = []
        for cmd in self.QUICK_COMMANDS:
            btn = QPushButton(cmd)
            btn.setObjectName("quickCmdButton")
            quick_row.addWidget(btn)
            self._quick_cmd_buttons.append(btn)
        quick_row.addStretch(1)
        self._quick_cmd_label = quick_label
        layout.addLayout(quick_row)

        self.console = QTextEdit()
        self.console.setReadOnly(True)
        self.console.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        self.console.document().setMaximumBlockCount(5000)
        layout.addWidget(self.console, 1)

        # Console toolbar (clear, autoscroll, byte counters)
        toolbar = QFrame()
        toolbar.setObjectName("consoleToolbar")
        toolbar_layout = QHBoxLayout(toolbar)
        toolbar_layout.setContentsMargins(SPACING_SM, SPACING_XS, SPACING_SM, SPACING_XS)
        toolbar_layout.setSpacing(SPACING_MD)
        self.console_clear_btn = QPushButton()
        self.console_clear_btn.setObjectName("iconButton")
        self.autoscroll_chk = QCheckBox()
        self.autoscroll_chk.setChecked(True)
        toolbar_layout.addWidget(self.console_clear_btn)
        toolbar_layout.addWidget(self.autoscroll_chk)
        toolbar_layout.addStretch(1)
        self.rx_counter = QLabel("RX: 0")
        self.rx_counter.setObjectName("byteCounter")
        self.tx_counter = QLabel("TX: 0")
        self.tx_counter.setObjectName("byteCounter")
        toolbar_layout.addWidget(self.rx_counter)
        toolbar_layout.addWidget(self.tx_counter)
        layout.addWidget(toolbar)

        # Send row
        send_row = QHBoxLayout()
        self.command = QLineEdit()
        self.send_btn = QPushButton()
        send_row.addWidget(self.command, 1)
        send_row.addWidget(self.send_btn)
        layout.addLayout(send_row)
        self.plot_tabs.addTab(page, "")

    @staticmethod
    def _spin(low: float, high: float, value: float, decimals: int) -> QDoubleSpinBox:
        widget = QDoubleSpinBox()
        widget.setRange(low, high)
        widget.setDecimals(decimals)
        widget.setValue(value)
        widget.setKeyboardTracking(False)
        return widget

    # ════════════════════════════════════════════════════════════════
    #  Wiring & shortcuts
    # ════════════════════════════════════════════════════════════════

    def _wire(self) -> None:
        self.refresh_btn.clicked.connect(self.refresh_ports)
        self.connect_btn.clicked.connect(self.toggle_connection)
        self.lang_box.currentIndexChanged.connect(self.change_language)
        self.emergency_btn.clicked.connect(self.emergency_stop)
        self.emergency_shortcut = QShortcut(QKeySequence("Ctrl+Shift+Space"), self)
        self.emergency_shortcut.activated.connect(self._emergency_from_shortcut_handler)
        self.motor.currentIndexChanged.connect(self.on_motor_changed)
        self.apply_btn.clicked.connect(self.apply_control)
        self.run_btn.clicked.connect(self.run_motor)
        self.stop_btn.clicked.connect(self.stop_motor)
        self.ff_apply.clicked.connect(self.apply_ff)
        self.motion_start.clicked.connect(self.start_motion)
        self.abort_btn.clicked.connect(self.emergency_stop)
        self.stream_on.clicked.connect(lambda: self.send(CommandBuilder.stream(True)))
        self.stream_off.clicked.connect(lambda: self.send(CommandBuilder.stream(False)))
        self.pause.toggled.connect(self.plot.set_paused)
        self.clear_btn.clicked.connect(self.plot.clear)
        self.export_btn.clicked.connect(self.export_csv)
        self.focus_btn.toggled.connect(self.set_focus_mode)
        self.focus_shortcut = QShortcut(QKeySequence("F11"), self)
        self.focus_shortcut.activated.connect(lambda: self.focus_btn.toggle())
        self.restore_shortcut = QShortcut(QKeySequence("Escape"), self)
        self.restore_shortcut.activated.connect(self.leave_focus_mode)

        # Sidebar toggle shortcuts
        self.toggle_left_shortcut = QShortcut(QKeySequence("Ctrl+["), self)
        self.toggle_left_shortcut.activated.connect(self._toggle_left_sidebar)
        self.toggle_right_shortcut = QShortcut(QKeySequence("Ctrl+]"), self)
        self.toggle_right_shortcut.activated.connect(self._toggle_right_sidebar)

        # Tab switch shortcuts
        self.tab_shortcut_1 = QShortcut(QKeySequence("Ctrl+1"), self)
        self.tab_shortcut_1.activated.connect(lambda: self.control_tabs.setCurrentIndex(0))
        self.tab_shortcut_2 = QShortcut(QKeySequence("Ctrl+2"), self)
        self.tab_shortcut_2.activated.connect(lambda: self.control_tabs.setCurrentIndex(1))
        self.tab_shortcut_3 = QShortcut(QKeySequence("Ctrl+3"), self)
        self.tab_shortcut_3.activated.connect(lambda: self.control_tabs.setCurrentIndex(2))
        self.plot_tab_shortcut_1 = QShortcut(QKeySequence("Ctrl+Shift+1"), self)
        self.plot_tab_shortcut_1.activated.connect(lambda: self.plot_tabs.setCurrentIndex(0))
        self.plot_tab_shortcut_2 = QShortcut(QKeySequence("Ctrl+Shift+2"), self)
        self.plot_tab_shortcut_2.activated.connect(lambda: self.plot_tabs.setCurrentIndex(1))

        # Rail buttons
        self.rail_motor_btn.clicked.connect(lambda: self._rail_tab_clicked(0))
        self.rail_motion_btn.clicked.connect(lambda: self._rail_tab_clicked(1))
        self.rail_device_btn.clicked.connect(lambda: self._rail_tab_clicked(2))
        self.rail_expand_btn.clicked.connect(self._expand_left_from_rail)
        self.right_rail_expand_btn.clicked.connect(self._expand_right_from_rail)

        # Telemetry options
        self.window.valueChanged.connect(self.plot.set_window)
        self.rpm_series.toggled.connect(lambda value: self.plot.set_group_visible("rpm", value))
        self.target_series.toggled.connect(lambda value: self.plot.set_group_visible("target", value))
        self.control_series.toggled.connect(lambda value: self.plot.set_group_visible("control", value))
        self.plot.frame_updated.connect(self.on_telemetry_frame)
        self.plot.render_rate_updated.connect(self.on_render_rate)

        # Console
        self.send_btn.clicked.connect(self.send_console)
        self.command.returnPressed.connect(self.send_console)
        self.command.installEventFilter(self)
        self.console_clear_btn.clicked.connect(self._clear_console)
        self.autoscroll_chk.toggled.connect(self._on_autoscroll_toggled)

        # Quick command buttons
        for btn in self._quick_cmd_buttons:
            btn.clicked.connect(lambda checked=False, cmd=btn.text(): self._send_quick_command(cmd))

        # Device tab
        self.query_btn.clicked.connect(self.query_device)
        self.save_btn.clicked.connect(self.save_config)
        self.load_btn.clicked.connect(self.load_config)

        # Track unsaved changes
        for widget in (self.kp, self.ki, self.kd, self.target, self.ff_k, self.ff_b):
            widget.valueChanged.connect(self._mark_unsaved)
        self.ff_enable.toggled.connect(self._mark_unsaved)
        self.motor.currentIndexChanged.connect(lambda _: self._mark_unsaved())

    # ════════════════════════════════════════════════════════════════
    #  Event filter for console command history navigation
    # ════════════════════════════════════════════════════════════════

    def eventFilter(self, obj, event):
        from PySide6.QtCore import QEvent
        if obj is self.command and event.type() == QEvent.Type.KeyPress:
            key = event.key()
            if key == Qt.Key.Key_Up:
                self._navigate_history(-1)
                return True
            elif key == Qt.Key.Key_Down:
                self._navigate_history(1)
                return True
        return super().eventFilter(obj, event)

    def _navigate_history(self, direction: int) -> None:
        if not self._console_history:
            return
        if self._console_history_index == -1:
            if direction == -1:
                self._console_history_index = len(self._console_history) - 1
            else:
                return
        else:
            self._console_history_index += direction
            if self._console_history_index < 0:
                self._console_history_index = 0
                return
            if self._console_history_index >= len(self._console_history):
                self._console_history_index = -1
                self.command.clear()
                return
        if self._console_history_index >= 0:
            self.command.setText(self._console_history[self._console_history_index])

    # ════════════════════════════════════════════════════════════════
    #  Console enhancements
    # ════════════════════════════════════════════════════════════════

    def _send_quick_command(self, cmd: str) -> None:
        """Send a quick command directly without duplicating the send path."""
        if not self.send(cmd, require_ready=False):
            return
        # Record in history
        if not self._console_history or self._console_history[-1] != cmd:
            self._console_history.append(cmd)
        self._console_history_index = -1

    def _clear_console(self) -> None:
        self.console.clear()
        self._rx_bytes = 0
        self._tx_bytes = 0
        self._update_byte_counters()

    def _on_autoscroll_toggled(self, checked: bool) -> None:
        self._autoscroll = checked

    def _update_byte_counters(self) -> None:
        self.rx_counter.setText(f"{tr(self.lang, 'rx_bytes')}: {self._rx_bytes}")
        self.tx_counter.setText(f"{tr(self.lang, 'tx_bytes')}: {self._tx_bytes}")

    # ════════════════════════════════════════════════════════════════
    #  Unsaved changes tracking
    # ════════════════════════════════════════════════════════════════

    def _mark_unsaved(self) -> None:
        self._has_unsaved_changes = True
        self._update_tab_badge()

    def _clear_unsaved(self) -> None:
        self._has_unsaved_changes = False
        self._update_tab_badge()

    def _update_tab_badge(self) -> None:
        """Show a small dot on the Motor tab when there are unsaved changes."""
        index = 0  # Motor tab
        base_text = tr(self.lang, 'motor')
        if self._has_unsaved_changes:
            base_text += " ●"  # dot badge
        self.control_tabs.setTabText(index, base_text)

    # ════════════════════════════════════════════════════════════════
    #  Sidebar management (left rail + right rail)
    # ════════════════════════════════════════════════════════════════

    def _toggle_left_sidebar(self) -> None:
        """Toggle left panel via Ctrl+[ or rail button."""
        new_state = self.control_tabs.isVisible()
        self.set_left_panel_visible(new_state)

    def _toggle_right_sidebar(self) -> None:
        """Toggle right panel via Ctrl+]."""
        new_state = self.live_table.isVisible()
        self.set_right_panel_visible(new_state)

    def _rail_tab_clicked(self, tab_index: int) -> None:
        """When a rail icon is clicked, expand sidebar and switch tab."""
        if not self.control_tabs.isVisible():
            self.set_left_panel_visible(True)
        self.control_tabs.setCurrentIndex(tab_index)

    def _expand_left_from_rail(self) -> None:
        self.set_left_panel_visible(True)

    def _expand_right_from_rail(self) -> None:
        self.set_right_panel_visible(True)

    def set_left_panel_visible(self, visible: bool) -> None:
        if not visible and self.control_tabs.isVisible():
            self._main_splitter_sizes = self.workspace_splitter.sizes()
        target = max(360, self._main_splitter_sizes[0])
        self._animate_sidebar(self.control_tabs, visible, target, minimum=360)
        self.left_rail.setVisible(not visible)
        if visible:
            self.workspace_splitter.setSizes(self._main_splitter_sizes)

    def set_right_panel_visible(self, visible: bool) -> None:
        if not visible and self.live_table.isVisible():
            self._telemetry_splitter_sizes = self.telemetry_splitter.sizes()
        target = max(220, self._telemetry_splitter_sizes[1])
        self._animate_sidebar(self.live_table, visible, target)
        self.right_rail.setVisible(not visible)
        if visible:
            self.telemetry_splitter.setSizes(self._telemetry_splitter_sizes)

    def _animate_sidebar(
        self, widget: QWidget, visible: bool, target: int, minimum: int = 0
    ) -> None:
        current = self._sidebar_animations.pop(widget, None)
        if current is not None:
            current.stop()
        if not self._animations_enabled():
            widget.setMinimumWidth(minimum if visible else 0)
            widget.setMaximumWidth(16_777_215)
            widget.setVisible(visible)
            return

        widget.setVisible(True)
        widget.setMinimumWidth(0)
        start = max(0, widget.width()) if not visible else 0
        animation = QPropertyAnimation(widget, b"maximumWidth", self)
        animation.setDuration(240)
        curve = QEasingCurve(QEasingCurve.Type.BezierSpline)
        if visible:
            curve.addCubicBezierSegment(
                QPointF(0.22, 1.0), QPointF(0.36, 1.0), QPointF(1.0, 1.0)
            )
        else:
            curve.addCubicBezierSegment(
                QPointF(0.40, 0.0), QPointF(1.0, 1.0), QPointF(1.0, 1.0)
            )
        animation.setEasingCurve(curve)
        animation.setStartValue(start)
        animation.setEndValue(target if visible else 0)

        def finish() -> None:
            widget.setVisible(visible)
            widget.setMinimumWidth(minimum if visible else 0)
            widget.setMaximumWidth(16_777_215)
            self._sidebar_animations.pop(widget, None)

        animation.finished.connect(finish)
        self._sidebar_animations[widget] = animation
        animation.start()

    @staticmethod
    def _animations_enabled() -> bool:
        try:
            import ctypes
            import sys

            if sys.platform == "win32":
                enabled = ctypes.c_int(1)
                if ctypes.windll.user32.SystemParametersInfoW(
                    0x1042, 0, ctypes.byref(enabled), 0
                ):
                    return bool(enabled.value)
        except (AttributeError, OSError):
            pass
        return True

    # ════════════════════════════════════════════════════════════════
    #  Focus mode
    # ════════════════════════════════════════════════════════════════

    def set_focus_mode(self, enabled: bool) -> None:
        if enabled == self._focus_mode:
            return
        self._focus_mode = enabled
        if enabled:
            self._window_was_maximized = self.isMaximized()
            self._pre_focus_left_visible = not self.left_rail.isVisible()
            self._pre_focus_right_visible = not self.right_rail.isVisible()
            if self.control_tabs.isVisible():
                self.set_left_panel_visible(False)
            if self.live_table.isVisible():
                self.set_right_panel_visible(False)
            self.conn_box.hide()
            self.safety.hide()
            self.plot_tabs.setCurrentIndex(0)
            self.showFullScreen()
        else:
            self.conn_box.show()
            self.safety.show()
            if self._pre_focus_left_visible:
                self.set_left_panel_visible(True)
            if self._pre_focus_right_visible:
                self.set_right_panel_visible(True)
            if self._window_was_maximized:
                self.showMaximized()
            else:
                self.showNormal()
        self.focus_btn.setChecked(enabled)
        self._update_view_controls()

    def leave_focus_mode(self) -> None:
        if self._focus_mode:
            self.focus_btn.setChecked(False)

    def _update_view_controls(self) -> None:
        self.focus_btn.setIcon(svg_icon("restore" if self._focus_mode else "focus", size=16))
        self.focus_btn.setToolTip(tr(self.lang, "focus_hint"))

    # ════════════════════════════════════════════════════════════════
    #  Pulse animation (connecting state + emergency button)
    # ════════════════════════════════════════════════════════════════

    def _on_pulse_tick(self) -> None:
        self._pulse_step = (self._pulse_step + 1) % 2
        # Toggle pulse property for status badge (connecting/handshake)
        if self.connection_state in ("connecting", "handshake"):
            self.status.setProperty("pulse", "on" if self._pulse_step else "off")
            self.status.style().unpolish(self.status)
            self.status.style().polish(self.status)
        # Emergency button subtle breathing (property-based)
        if self.worker is not None:
            self.emergency_btn.setProperty("pulse", "on" if self._pulse_step else "off")
            self.emergency_btn.style().unpolish(self.emergency_btn)
            self.emergency_btn.style().polish(self.emergency_btn)

    # ════════════════════════════════════════════════════════════════
    #  Connection duration timer
    # ════════════════════════════════════════════════════════════════

    def _update_duration(self) -> None:
        if self._connect_start_time is None:
            self.duration_label.setText("")
            return
        elapsed = int(time.monotonic() - self._connect_start_time)
        minutes, seconds = divmod(elapsed, 60)
        hours, minutes = divmod(minutes, 60)
        prefix = tr(self.lang, "duration") + ": "
        if hours:
            self.duration_label.setText(f"{prefix}{hours}:{minutes:02d}:{seconds:02d}")
        else:
            self.duration_label.setText(f"{prefix}{minutes:02d}:{seconds:02d}")

    # ════════════════════════════════════════════════════════════════
    #  Serial connection
    # ════════════════════════════════════════════════════════════════

    def refresh_ports(self) -> None:
        selected_port = self.port.currentData()
        self.port.clear()
        try:
            from serial.tools import list_ports

            ports = sorted(list_ports.comports(), key=lambda item: item.device)
            for item in ports:
                port_icon = svg_icon("dot_online", size=10) if item.description else svg_icon("dot_offline", size=10)
                self.port.addItem(f"{item.device} — {item.description}", item.device)
                self.port.setItemIcon(self.port.count() - 1, port_icon)
        except Exception as exc:
            self._append_console(f"Serial scan failed: {exc}", "RX")
        if selected_port:
            index = self.port.findData(selected_port)
            if index >= 0:
                self.port.setCurrentIndex(index)
        self._update_connection_controls()

    def toggle_connection(self) -> None:
        if self.worker is not None:
            # Check if any motors are running before disconnecting
            if self.motor_statuses:
                any_running = any(
                    status.enabled for status in self.motor_statuses.values()
                )
                if any_running:
                    answer = QMessageBox.question(
                        self,
                        tr(self.lang, "disconnect"),
                        tr(self.lang, "confirm_disconnect_motors"),
                    )
                    if answer != QMessageBox.StandardButton.Yes:
                        return
            self.disconnect_serial()
            return
        port = self.port.currentData()
        if not port:
            self.statusBar().showMessage(tr(self.lang, "connection_failed").format(detail=tr(self.lang, "no_port")), 5000)
            return
        worker = SerialWorker(port, int(self.baud.currentText()), self)
        worker.line_received.connect(self.on_line)
        worker.connected.connect(self.on_connected)
        worker.disconnected.connect(self.on_disconnected)
        worker.failed.connect(self.on_serial_error)
        self.worker = worker
        self.connection_state = "connecting"
        self.handshake_ok = False
        self.machine.clear()
        self.motor_statuses.clear()
        self._rx_bytes = 0
        self._tx_bytes = 0
        self._update_byte_counters()
        self.retranslate()
        worker.start()

    def disconnect_serial(self) -> bool:
        worker = self.worker
        if worker is None:
            return True
        self._disconnecting = True
        self.handshake_timer.stop()
        self._pulse_timer.stop()
        self._duration_timer.stop()
        self._connect_start_time = None
        self._update_duration()
        worker.shutdown((CommandBuilder.stop_all(), CommandBuilder.stream(False)))
        stopped = worker.wait(2000)
        if not stopped:
            worker.requestInterruption()
            stopped = worker.wait(1000)
        if not stopped:
            self._disconnecting = False
            self.on_error("Serial worker did not stop; the window remains open for safety.")
            return False
        worker.deleteLater()
        self.worker = None
        self.connected_port = ""
        self.handshake_ok = False
        self.streaming = False
        self.connection_state = "disconnected"
        self.dtr_dot.setVisible(False)
        self.rts_dot.setVisible(False)
        self.port_info_label.setText("")
        self._disconnecting = False
        self.retranslate()
        return True

    def on_connected(self, port: str) -> None:
        self.connected_port = port
        self.connection_state = "handshake"
        self.handshake_timer.start(self.HANDSHAKE_TIMEOUT_MS)
        self._connect_start_time = time.monotonic()
        self._duration_timer.start()
        self._pulse_timer.start()
        # Show DTR/RTS indicators
        self.dtr_dot.setVisible(True)
        self.rts_dot.setVisible(True)
        self.dtr_dot.setPixmap(svg_pixmap("dot_online", size=10, color=Palette.DOT_ACTIVE))
        self.rts_dot.setPixmap(svg_pixmap("dot_online", size=10, color=Palette.DOT_ACTIVE))
        # Update port info in status bar
        self.port_info_label.setText(
            tr(self.lang, "port_info").format(port=port, baud=self.baud.currentText())
        )
        self.retranslate()
        self.send(CommandBuilder.info_query(), require_ready=False)

    def on_disconnected(self) -> None:
        if self._disconnecting:
            return
        self.handshake_timer.stop()
        self._pulse_timer.stop()
        self._duration_timer.stop()
        self._connect_start_time = None
        self._update_duration()
        self.worker = None
        self.connected_port = ""
        self.handshake_ok = False
        self.streaming = False
        self.connection_state = "disconnected"
        self.dtr_dot.setVisible(False)
        self.rts_dot.setVisible(False)
        self.port_info_label.setText("")
        self.retranslate()

    def on_serial_error(self, message: str) -> None:
        self.connection_state = "incompatible"
        self._pulse_timer.stop()
        self.retranslate()
        QMessageBox.critical(
            self,
            tr(self.lang, "error"),
            tr(self.lang, "connection_failed").format(detail=message),
        )

    def _handshake_timeout(self) -> None:
        if self.handshake_ok or not self.worker:
            return
        self.connection_state = "incompatible"
        self._pulse_timer.stop()
        self.retranslate()
        QMessageBox.warning(self, tr(self.lang, "error"), tr(self.lang, "handshake_timeout"))

    def on_error(self, message: str) -> None:
        QMessageBox.critical(self, tr(self.lang, "error"), message)

    def send(self, command: str, require_ready: bool = True) -> bool:
        worker = self.worker
        if worker is None:
            return False
        if require_ready and not self.handshake_ok:
            self.statusBar().showMessage(tr(self.lang, "not_ready"), 4000)
            return False
        try:
            worker.send(command)
            self._append_console(command, "TX")
            return True
        except Exception as exc:
            self.on_error(str(exc))
            return False

    def _emergency_from_shortcut_handler(self) -> None:
        """Handle emergency stop triggered via keyboard shortcut.

        When the window is not active (shortcut fires globally), show a
        confirmation dialog to prevent accidental triggers.
        """
        if not self.isActiveWindow():
            self._emergency_from_shortcut = True
            answer = QMessageBox.question(
                self,
                tr(self.lang, "stop_all"),
                tr(self.lang, "confirm_emergency"),
            )
            if answer != QMessageBox.StandardButton.Yes:
                self._emergency_from_shortcut = False
                return
            self._emergency_from_shortcut = False
        self.emergency_stop()

    def emergency_stop(self) -> None:
        worker = self.worker
        if worker is None:
            return
        try:
            worker.emergency_stop()
            self.streaming = False
            self._append_console("StopAll + Stream=0", "URGENT")
            self.statusBar().showMessage(tr(self.lang, "stop_all"), 5000)
        except Exception as exc:
            self.on_error(str(exc))

    def send_console(self) -> None:
        text = self.command.text().strip()
        if text and self.send(text):
            # Add to history
            if not self._console_history or self._console_history[-1] != text:
                self._console_history.append(text)
            self._console_history_index = -1
            self.command.clear()

    def _append_console(self, text: str, direction: str = "RX") -> None:
        colors = {"TX": "#38BDF8", "RX": "#CBD5E1", "URGENT": "#F87171"}
        safe = html.escape(text)
        timestamp = time.strftime("%H:%M:%S")
        self.console.append(
            f'<span style="color:{colors.get(direction, colors["RX"])}">'
            f'<span style="color:#64748B;">[{timestamp}]</span> '
            f"<b>{direction}:</b> {safe}</span>"
        )
        # Count bytes by direction
        byte_len = len(text.encode("utf-8"))
        if direction == "TX":
            self._tx_bytes += byte_len
        else:
            self._rx_bytes += byte_len
        self._update_byte_counters()
        if self._autoscroll:
            self.console.verticalScrollBar().setValue(
                self.console.verticalScrollBar().maximum()
            )

    # ════════════════════════════════════════════════════════════════
    #  Protocol handling
    # ════════════════════════════════════════════════════════════════

    def on_line(self, line: str) -> None:
        decoded = decode_line(line)
        if decoded.kind is LineKind.TELEMETRY:
            assert decoded.telemetry is not None
            self.plot.add_frame(decoded.telemetry)
            return
        if decoded.kind is LineKind.EMPTY:
            return

        self._append_console(line)
        if decoded.kind is LineKind.TEXT:
            lowered = line.lower()
            if any(token in lowered for token in ("rejected", "failed", "fault", "error")):
                self.statusBar().showMessage(
                    tr(self.lang, "command_rejected").format(detail=line), 7000
                )
            return

        assert decoded.machine is not None
        message = decoded.machine
        self.machine[message.topic] = dict(message.fields)
        self._refresh_raw_protocol()
        try:
            if message.topic == "INFO":
                self._handle_info(message)
            elif message.topic == "STATUS":
                status = parse_motor_status(message)
                self.motor_statuses[status.motor] = status
                if status.motor == self.motor.currentIndex():
                    self._display_motor_status(status)
            elif message.topic == "ACK" and message.fields.get("cmd") == "stream":
                self.streaming = message.fields.get("state") == "1"
                self.statusBar().showMessage(
                    tr(self.lang, "stream_on" if self.streaming else "stream_off"),
                    3000,
                )
        except ValueError as exc:
            self.statusBar().showMessage(
                tr(self.lang, "device_invalid").format(detail=str(exc)), 7000
            )

    def _handle_info(self, message) -> None:
        try:
            info = parse_device_info(message)
        except ValueError as exc:
            self.connection_state = "incompatible"
            self.retranslate()
            QMessageBox.warning(
                self, tr(self.lang, "error"),
                tr(self.lang, "device_invalid").format(detail=str(exc)),
            )
            return
        if info.protocol != PROTOCOL_VERSION:
            self.connection_state = "incompatible"
            self.retranslate()
            QMessageBox.warning(
                self, tr(self.lang, "error"),
                tr(self.lang, "protocol_mismatch").format(
                    expected=PROTOCOL_VERSION, actual=info.protocol
                ),
            )
            return
        if info.motors != MOTOR_COUNT or info.telemetry.lower() != "firewater":
            self.connection_state = "incompatible"
            self.retranslate()
            QMessageBox.warning(
                self, tr(self.lang, "error"),
                tr(self.lang, "device_invalid").format(
                    detail=f"motors={info.motors}, telemetry={info.telemetry}"
                ),
            )
            return

        self.device_info = info
        self.handshake_timer.stop()
        self._pulse_timer.stop()
        # Keep pulse timer running for emergency button breathing
        if self.worker is not None:
            self._pulse_timer.start()
        self.handshake_ok = True
        self.connection_state = "ready"
        self.device.setText(f"{info.board} · FW {info.firmware} · P{info.protocol}")
        self.device_summary.setText(
            f"{info.board}  |  {info.driver}  |  {info.motors} motors\n"
            f"FW {info.firmware}  |  Protocol {info.protocol}  |  {info.baud} baud  |  {info.telemetry}"
        )
        self.retranslate()
        self.query_device()

    def _refresh_raw_protocol(self) -> None:
        rows = []
        for topic, fields in self.machine.items():
            rows.append(f"@{topic}\n  " + "\n  ".join(f"{key}: {value}" for key, value in fields.items()))
        self.info_text.setPlainText("\n\n".join(rows))

    def query_device(self) -> None:
        if not self.handshake_ok:
            return
        self.send(CommandBuilder.config_query())
        selected = self.motor.currentIndex()
        order = [motor for motor in range(MOTOR_COUNT) if motor != selected] + [selected]
        for motor in order:
            self.send(CommandBuilder.status(motor))

    def on_motor_changed(self, motor_id: int) -> None:
        status = self.motor_statuses.get(motor_id)
        if status is not None:
            self._display_motor_status(status)
        else:
            self._clear_motor_status()
        if self.handshake_ok:
            self.send(CommandBuilder.status(motor_id))
        self._update_motor_indicator()

    def _display_motor_status(self, status: MotorStatus) -> None:
        self.kp.setValue(status.kp)
        self.ki.setValue(status.ki)
        self.kd.setValue(status.kd)
        self.target.setValue(status.target)
        self.ff_k.setValue(status.ff_k)
        self.ff_b.setValue(status.ff_b)
        self.ff_enable.setChecked(status.ff_enabled)
        mode_index = self.mode.findData(status.mode)
        if mode_index >= 0:
            self.mode.setCurrentIndex(mode_index)
        state_key = "enabled_state" if status.enabled else "stopped_state"
        self.motor_state.setText(tr(self.lang, state_key))
        self.motor_state.setProperty("active", status.enabled)
        self.motor_state.style().unpolish(self.motor_state)
        self.motor_state.style().polish(self.motor_state)

        # RPM colour based on direction
        rpm_text = f"{status.rpm:.0f} RPM"
        self.motor_rpm.setText(rpm_text)
        if status.rpm > 0.5:
            self.motor_rpm.setStyleSheet(f"color: {Palette.RPM_FORWARD};")
        elif status.rpm < -0.5:
            self.motor_rpm.setStyleSheet(f"color: {Palette.RPM_REVERSE};")
        else:
            self.motor_rpm.setStyleSheet(f"color: {Palette.RPM_STOPPED};")

        self.motor_output.setText(f"{status.output:.0f}")
        self.motor_mode.setText(tr(self.lang, status.mode))

        # Deviation percentage
        if abs(status.target) > 0.1:
            deviation = ((status.rpm - status.target) / abs(status.target)) * 100.0
            self.motor_deviation.setText(
                f"{tr(self.lang, 'rpm_deviation')}: {deviation:+.1f}%"
            )
        elif abs(status.rpm) > 0.5:
            self.motor_deviation.setText(
                f"{tr(self.lang, 'rpm_deviation')}: —"
            )
        else:
            self.motor_deviation.setText(
                f"{tr(self.lang, 'rpm_deviation')}: 0.0%"
            )

        # Motor colour bar on state box
        self.motor_state_box.setProperty("motorColor", str(status.motor))
        self.motor_state_box.style().unpolish(self.motor_state_box)
        self.motor_state_box.style().polish(self.motor_state_box)

        # Update right rail summary
        self.right_rail_rpm_label.setText(f"{status.rpm:.0f}")
        self.right_rail_state_label.setText(tr(self.lang, state_key))
        self.right_rail_state_label.setProperty("state", "ready" if status.enabled else "disconnected")
        self.right_rail_state_label.style().unpolish(self.right_rail_state_label)
        self.right_rail_state_label.style().polish(self.right_rail_state_label)
        # Update window title
        self._update_window_title()
        self._update_motor_indicator()
        # Clear unsaved flag since we just loaded from device
        self._clear_unsaved()

    def _clear_motor_status(self) -> None:
        self.motor_state.setText("—")
        self.motor_rpm.setText("— RPM")
        self.motor_rpm.setStyleSheet("")
        self.motor_output.setText("—")
        self.motor_mode.setText("—")
        self.motor_deviation.setText("—")
        self.motor_state_box.setProperty("motorColor", str(self.motor.currentIndex()))
        self.motor_state_box.style().unpolish(self.motor_state_box)
        self.motor_state_box.style().polish(self.motor_state_box)
        self.right_rail_rpm_label.setText("—")
        self.right_rail_state_label.setText("—")
        self._update_window_title()
        self._update_motor_indicator()

    def _update_motor_indicator(self) -> None:
        """Show current motor letter in the status bar."""
        motor_letter = "ABCD"[self.motor.currentIndex()]
        self.motor_indicator.setText(
            tr(self.lang, "motor_label_short").format(letter=motor_letter)
        )

    def _update_window_title(self) -> None:
        """Show connection status and current motor in the window title."""
        motor_letter = "ABCD"[self.motor.currentIndex()]
        if self.handshake_ok:
            title = f"{tr(self.lang, 'title')}  ·  [{motor_letter}] {tr(self.lang, 'ready')}"
        elif self.worker is not None:
            title = f"{tr(self.lang, 'title')}  ·  {tr(self.lang, self.connection_state)}"
        else:
            title = tr(self.lang, "title")
        self.setWindowTitle(title)

    def apply_control(self, query_after: bool = True) -> None:
        motor_id = self.motor.currentIndex()
        self.send(CommandBuilder.motor(motor_id))
        for name, widget in (("kp", self.kp), ("ki", self.ki), ("kd", self.kd)):
            self.send(CommandBuilder.pid(name, widget.value()))
        self.send(CommandBuilder.target(self.target.value()))
        if query_after:
            self.send(CommandBuilder.status(motor_id))
        self._clear_unsaved()

    def run_motor(self) -> None:
        motor_id = self.motor.currentIndex()
        answer = QMessageBox.question(
            self,
            tr(self.lang, "run"),
            tr(self.lang, "confirm_run").format(
                motor="ABCD"[motor_id],
                rpm=self.target.value(),
                kp=self.kp.value(),
                ki=self.ki.value(),
                kd=self.kd.value(),
            ),
        )
        if answer == QMessageBox.StandardButton.Yes:
            self.apply_control(query_after=False)
            self.send(CommandBuilder.run(motor_id))
            self.streaming = True
            self.send(CommandBuilder.status(motor_id))

    def stop_motor(self) -> None:
        motor_id = self.motor.currentIndex()
        self.send(CommandBuilder.stop(motor_id))
        self.streaming = False
        self.send(CommandBuilder.status(motor_id))

    def apply_ff(self) -> None:
        self.send(CommandBuilder.motor(self.motor.currentIndex()))
        for command in CommandBuilder.feedforward(
            self.ff_k.value(), self.ff_b.value(), self.ff_enable.isChecked()
        ):
            self.send(command)
        self.send(CommandBuilder.status(self.motor.currentIndex()))
        self._clear_unsaved()

    def start_motion(self) -> None:
        mode = self.mode.currentData()
        if mode == "speed":
            self.run_motor()
            return
        target = (
            f"{self.pulses.value():.0f} pulses"
            if mode == "position"
            else f"{self.degrees.value():.1f}°"
        )
        answer = QMessageBox.question(
            self,
            tr(self.lang, "start"),
            tr(self.lang, "confirm_motion").format(
                mode=tr(self.lang, mode), target=target
            ),
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        self.send(CommandBuilder.mode(mode))
        if mode == "position":
            self.send(CommandBuilder.position(self.pulses.value(), self.cruise.value()))
        else:
            self.send(CommandBuilder.angle(self.degrees.value(), self.cruise.value()))

    # ════════════════════════════════════════════════════════════════
    #  Telemetry callbacks
    # ════════════════════════════════════════════════════════════════

    def on_telemetry_frame(self, values: dict[str, float], rate: float) -> None:
        self._input_rate = rate
        self._update_rate_badge()
        for row, name in enumerate(CHANNEL_NAMES):
            self.live_table.item(row, 1).setText(f"{values[name]:.3f}")

    def on_render_rate(self, rate: float) -> None:
        self._render_rate = rate
        self._update_rate_badge()

    def _update_rate_badge(self) -> None:
        self.sample_rate.setText(
            f"RX {self._input_rate:.1f} Hz · UI {self._render_rate:.0f} FPS · {len(self.plot.rows)}"
        )

    def export_csv(self) -> None:
        if not self.plot.rows:
            QMessageBox.information(self, tr(self.lang, "export"), tr(self.lang, "no_data"))
            return
        path, _ = QFileDialog.getSaveFileName(
            self, tr(self.lang, "export"), "telemetry.csv", "CSV (*.csv)"
        )
        if path:
            try:
                self.plot.export_csv(path)
            except OSError as exc:
                self.on_error(str(exc))
            else:
                self.statusBar().showMessage(tr(self.lang, "exported"), 4000)

    # ════════════════════════════════════════════════════════════════
    #  Settings & geometry persistence

    # ════════════════════════════════════════════════════════════════

    def collect_settings(self) -> AppSettings:
        return AppSettings(
            language=self.lang,
            baud=int(self.baud.currentText()),
            motor=self.motor.currentIndex(),
            kp=self.kp.value(), ki=self.ki.value(), kd=self.kd.value(),
            target_rpm=self.target.value(),
            ff_k=self.ff_k.value(), ff_b=self.ff_b.value(),
            ff_enabled=self.ff_enable.isChecked(),
            plot_window_s=self.window.value(),
            window_x=self.x(),
            window_y=self.y(),
            window_w=self.width(),
            window_h=self.height(),
            window_maximized=self.isMaximized(),
            left_panel_visible=self.control_tabs.isVisible(),
            right_panel_visible=self.live_table.isVisible(),
            main_splitter_sizes=tuple(self.workspace_splitter.sizes()),
            telemetry_splitter_sizes=tuple(self.telemetry_splitter.sizes()),
        )

    def _restore_geometry(self) -> None:
        s = self.settings
        if s.window_x >= 0 and s.window_y >= 0:
            self.move(s.window_x, s.window_y)
        self.resize(s.window_w, s.window_h)
        if s.window_maximized:
            self.showMaximized()
        if not s.left_panel_visible:
            # Defer to after show
            QTimer.singleShot(0, lambda: self.set_left_panel_visible(False))
        if not s.right_panel_visible:
            QTimer.singleShot(0, lambda: self.set_right_panel_visible(False))

    def _save_geometry_to_settings(self) -> None:
        s = self.settings
        s.window_x = max(0, self.x()) if not self.isMaximized() else s.window_x
        s.window_y = max(0, self.y()) if not self.isMaximized() else s.window_y
        s.window_w = self.width() if not self.isMaximized() else s.window_w
        s.window_h = self.height() if not self.isMaximized() else s.window_h
        s.window_maximized = self.isMaximized()
        s.left_panel_visible = self.control_tabs.isVisible()
        s.right_panel_visible = self.live_table.isVisible()
        s.main_splitter_sizes = tuple(self.workspace_splitter.sizes())
        s.telemetry_splitter_sizes = tuple(self.telemetry_splitter.sizes())
        s.language = self.lang
        s.motor = self.motor.currentIndex()

    def save_config(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, tr(self.lang, "save"), "mspm0-config.json", "JSON (*.json)"
        )
        if not path:
            return
        try:
            self.collect_settings().save(path)
        except Exception as exc:
            self.on_error(str(exc))
        else:
            self.statusBar().showMessage(tr(self.lang, "saved"), 4000)
            self._clear_unsaved()

    def load_config(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, tr(self.lang, "load"), "", "JSON (*.json)"
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
        self._clear_unsaved()
        self.retranslate()

    # ════════════════════════════════════════════════════════════════
    #  Language & retranslation
    # ════════════════════════════════════════════════════════════════

    def change_language(self, _index: int = -1) -> None:
        self.lang = self.lang_box.currentData() or "en"
        self.retranslate()
        status = self.motor_statuses.get(self.motor.currentIndex())
        if status:
            self._display_motor_status(status)

    def retranslate(self) -> None:
        self.setWindowTitle(tr(self.lang, "title"))
        self._update_window_title()
        self.conn_box.setTitle(tr(self.lang, "connection"))
        self.port_label.setText(tr(self.lang, "port"))
        self.port_label.setBuddy(self.port)
        self.refresh_btn.setIcon(svg_icon("refresh", size=16))
        self.refresh_btn.setText(tr(self.lang, "refresh"))
        self.refresh_btn.setToolTip(tr(self.lang, "refresh"))
        self.baud_label.setText(tr(self.lang, "baud"))
        self.baud_label.setBuddy(self.baud)
        self.connect_btn.setIcon(svg_icon("disconnect" if self.worker else "connect", size=16))
        self.connect_btn.setText(tr(self.lang, "disconnect") if self.worker else tr(self.lang, "connect"))
        self.connect_btn.setToolTip(tr(self.lang, "connect") if not self.worker else tr(self.lang, "disconnect"))
        # Connection button state colour
        self.connect_btn.setProperty("state", self.connection_state)
        self.connect_btn.style().unpolish(self.connect_btn)
        self.connect_btn.style().polish(self.connect_btn)
        self.language_label.setText(tr(self.lang, "language"))
        self.language_label.setBuddy(self.lang_box)
        self.status.setText(tr(self.lang, self.connection_state))
        self.status.setProperty("state", self.connection_state)
        self.status.style().unpolish(self.status)
        self.status.style().polish(self.status)
        self.safety.setText(tr(self.lang, "safety"))
        self.emergency_btn.setIcon(svg_icon("emergency_stop", size=20, color="#FFFFFF"))
        self.emergency_btn.setText(tr(self.lang, "stop_all"))
        self.emergency_btn.setToolTip(tr(self.lang, "emergency_hint"))
        self.motor_state_box.setTitle(tr(self.lang, "motor_state"))
        self._motor_state_caption.setText(tr(self.lang, "enabled_state"))
        self._motor_rpm_caption.setText(tr(self.lang, "rpm_caption"))
        self._motor_output_caption.setText(tr(self.lang, "output_state"))
        self._motor_mode_caption.setText(tr(self.lang, "mode_state"))
        self.motor_box.setTitle(tr(self.lang, "pid"))
        self.motor_label.setText(tr(self.lang, "motor"))
        self.motor_label.setBuddy(self.motor)
        # Retranslate motor combo items
        for i, key in enumerate(self.MOTOR_LABELS):
            self.motor.setItemText(i, tr(self.lang, key))
        self.target_label.setText(tr(self.lang, "target"))
        self.target_label.setBuddy(self.target)
        self.apply_btn.setIcon(svg_icon("apply", size=16))
        self.apply_btn.setText(tr(self.lang, "apply"))
        self.apply_btn.setToolTip(tr(self.lang, "apply"))
        self.run_btn.setIcon(svg_icon("run", size=16))
        self.run_btn.setText(tr(self.lang, "run"))
        self.run_btn.setToolTip(tr(self.lang, "run"))
        self.stop_btn.setIcon(svg_icon("stop", size=16))
        self.stop_btn.setText(tr(self.lang, "stop"))
        self.stop_btn.setToolTip(tr(self.lang, "stop"))
        self.ff_box.setTitle(tr(self.lang, "ff"))
        self.ff_enable.setText(tr(self.lang, "enabled"))
        self.ff_apply.setIcon(svg_icon("apply", size=16))
        self.ff_apply.setText(tr(self.lang, "apply"))
        self.ff_apply.setToolTip(tr(self.lang, "apply"))
        # Tab text with SVG icons
        self.control_tabs.setTabIcon(0, svg_icon("motor", size=16))
        self.control_tabs.setTabText(0, tr(self.lang, 'motor'))
        self.control_tabs.setTabIcon(1, svg_icon("motion", size=16))
        self.control_tabs.setTabText(1, tr(self.lang, 'motion'))
        self.control_tabs.setTabIcon(2, svg_icon("device", size=16))
        self.control_tabs.setTabText(2, tr(self.lang, 'device'))
        self.mode_label.setText(tr(self.lang, "mode"))
        for index, key in enumerate(("speed", "position", "angle")):
            self.mode.setItemText(index, tr(self.lang, key))
        self.pulses_label.setText(tr(self.lang, "pulses"))
        self.degrees_label.setText(tr(self.lang, "degrees"))
        self.cruise_label.setText(tr(self.lang, "cruise"))
        self.motion_start.setIcon(svg_icon("run", size=16))
        self.motion_start.setText(tr(self.lang, "start"))
        self.abort_btn.setIcon(svg_icon("stop", size=16))
        self.abort_btn.setText(tr(self.lang, "abort"))
        self.device_summary_label.setText(tr(self.lang, "device_summary"))
        self.raw_protocol_label.setText(tr(self.lang, "raw_protocol"))
        self.query_btn.setIcon(svg_icon("refresh", size=16))
        self.query_btn.setText(tr(self.lang, "query"))
        self.query_btn.setToolTip(tr(self.lang, "query"))
        # Telemetry toolbar — icon buttons with tooltips
        self.stream_on.setToolTip(tr(self.lang, "stream_on"))
        self.stream_off.setToolTip(tr(self.lang, "stream_off"))
        self.pause.setToolTip(tr(self.lang, "pause"))
        self.clear_btn.setToolTip(tr(self.lang, "clear"))
        self.export_btn.setToolTip(tr(self.lang, "export"))
        self._update_view_controls()
        self.window_label.setText(tr(self.lang, "seconds"))
        self.window_label.setBuddy(self.window)
        # Series checkboxes with colour indicators
        self.rpm_series.setText(tr(self.lang, "rpm_series"))
        self.target_series.setText(tr(self.lang, "target_series"))
        self.control_series.setText(tr(self.lang, "control_series"))
        self.save_btn.setIcon(svg_icon("save", size=16))
        self.save_btn.setText(tr(self.lang, "save"))
        self.save_btn.setToolTip(tr(self.lang, "save"))
        self.load_btn.setIcon(svg_icon("load", size=16))
        self.load_btn.setText(tr(self.lang, "load"))
        self.load_btn.setToolTip(tr(self.lang, "load"))
        self.command.setPlaceholderText(tr(self.lang, "console_hint"))
        self.send_btn.setIcon(svg_icon("send", size=16))
        self.send_btn.setText(tr(self.lang, "send"))
        self.send_btn.setToolTip(tr(self.lang, "send"))
        self.plot_tabs.setTabText(0, tr(self.lang, "telemetry"))
        self.plot_tabs.setTabText(1, tr(self.lang, "console"))
        # Rail tooltips
        self.rail_motor_btn.setToolTip(tr(self.lang, "rail_hint_motor"))
        self.rail_motion_btn.setToolTip(tr(self.lang, "rail_hint_motion"))
        self.rail_device_btn.setToolTip(tr(self.lang, "rail_hint_device"))
        self.rail_expand_btn.setToolTip(tr(self.lang, "rail_expand"))
        self.right_rail_expand_btn.setToolTip(tr(self.lang, "right_rail_expand"))
        self.right_rail_rpm_caption.setText(tr(self.lang, "right_rail_rpm"))
        # Motor combo box color indicators via tooltip
        for i in range(MOTOR_COUNT):
            self.motor.setItemData(i, Palette.MOTOR_COLORS[i], Qt.ItemDataRole.ToolTipRole)
        # Console enhancements
        self._quick_cmd_label.setText(tr(self.lang, "quick_commands"))
        self.console_clear_btn.setIcon(svg_icon("clear", size=16))
        self.console_clear_btn.setToolTip(tr(self.lang, "console_clear"))
        self.autoscroll_chk.setText(tr(self.lang, "autoscroll"))
        self._update_byte_counters()
        # Status bar
        self._update_motor_indicator()
        if self.connected_port:
            self.port_info_label.setText(
                tr(self.lang, "port_info").format(
                    port=self.connected_port, baud=self.baud.currentText()
                )
            )
        self._update_connection_controls()
        self._update_duration()

    def _update_connection_controls(self) -> None:
        connected = self.worker is not None
        ready = connected and self.handshake_ok
        self.port.setEnabled(not connected)
        self.baud.setEnabled(not connected)
        self.refresh_btn.setEnabled(not connected)
        self.connect_btn.setEnabled(connected or self.port.count() > 0)
        self.emergency_btn.setEnabled(connected)
        for widget in (
            self.apply_btn, self.run_btn, self.stop_btn, self.ff_apply,
            self.motion_start, self.abort_btn, self.query_btn,
            self.stream_on, self.stream_off, self.command, self.send_btn,
        ):
            widget.setEnabled(ready)
        # Quick command buttons
        for btn in self._quick_cmd_buttons:
            btn.setEnabled(ready)

    # ════════════════════════════════════════════════════════════════
    #  Tab order for keyboard navigation
    # ════════════════════════════════════════════════════════════════

    def _set_tab_order(self) -> None:
        """Establish a logical focus chain."""
        chain = [
            self.port, self.refresh_btn, self.baud, self.connect_btn,
            self.lang_box, self.emergency_btn,
            # Motor tab
            self.motor, self.kp, self.ki, self.kd, self.target,
            self.apply_btn, self.run_btn, self.stop_btn,
            self.ff_k, self.ff_b, self.ff_enable, self.ff_apply,
            # Motion tab
            self.mode, self.pulses, self.degrees, self.cruise,
            self.motion_start, self.abort_btn,
            # Device tab
            self.query_btn, self.save_btn, self.load_btn,
            # Telemetry
            self.stream_on, self.stream_off, self.pause,
            self.clear_btn, self.export_btn, self.focus_btn,
            self.window, self.rpm_series, self.target_series, self.control_series,
            # Console
            self.command, self.send_btn,
        ]
        for i in range(len(chain) - 1):
            self.setTabOrder(chain[i], chain[i + 1])

    # ════════════════════════════════════════════════════════════════
    #  Close event
    # ════════════════════════════════════════════════════════════════

    def closeEvent(self, event) -> None:
        self._save_geometry_to_settings()
        if self.disconnect_serial():
            event.accept()
        else:
            event.ignore()
