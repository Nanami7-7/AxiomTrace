from __future__ import annotations

import csv
import time
from collections import deque
from pathlib import Path

from PySide6.QtCharts import QChart, QChartView, QLineSeries, QValueAxis
from PySide6.QtCore import QPointF, Qt
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QVBoxLayout, QWidget

from .protocol import CHANNEL_NAMES, TelemetryFrame

COLORS = [
    "#e53935", "#1e88e5", "#43a047", "#8e24aa",
    "#ff8f00", "#00acc1", "#6d4c41", "#546e7a",
]


class TelemetryPlot(QWidget):
    """Plot four measured RPM and four target RPM channels."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.window_s = 15.0
        self.paused = False
        self.started = time.monotonic()
        self.rows: deque[tuple[float, tuple[float, ...]]] = deque(maxlen=20_000)

        self.chart = QChart()
        self.chart.legend().setVisible(True)
        self.axis_x = QValueAxis()
        self.axis_x.setTitleText("t (s)")
        self.axis_y = QValueAxis()
        self.axis_y.setTitleText("RPM")
        self.chart.addAxis(self.axis_x, Qt.AlignmentFlag.AlignBottom)
        self.chart.addAxis(self.axis_y, Qt.AlignmentFlag.AlignLeft)

        self.series: list[QLineSeries] = []
        for index, name in enumerate(CHANNEL_NAMES[:8]):
            series = QLineSeries()
            series.setName(name)
            series.setPen(QPen(QColor(COLORS[index]), 2.0))
            self.chart.addSeries(series)
            series.attachAxis(self.axis_x)
            series.attachAxis(self.axis_y)
            self.series.append(series)

        view = QChartView(self.chart)
        view.setRenderHint(QPainter.RenderHint.Antialiasing)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(view)

    def set_window(self, seconds: int) -> None:
        self.window_s = float(seconds)

    def clear(self) -> None:
        self.rows.clear()
        self.started = time.monotonic()
        for series in self.series:
            series.clear()

    def add_frame(self, frame: TelemetryFrame) -> None:
        t = time.monotonic() - self.started
        self.rows.append((t, frame.values))
        if self.paused:
            return

        cutoff = t - self.window_s
        visible = [row for row in self.rows if row[0] >= cutoff]
        for index, series in enumerate(self.series):
            points = [QPointF(x, values[index]) for x, values in visible]
            series.replace(points)

        self.axis_x.setRange(max(0.0, cutoff), max(self.window_s, t))
        values_y = [values[index] for _, values in visible for index in range(8)]
        if values_y:
            low, high = min(values_y), max(values_y)
            padding = max(10.0, (high - low) * 0.1)
            self.axis_y.setRange(low - padding, high + padding)

    def export_csv(self, path: str | Path) -> None:
        with Path(path).open("w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.writer(stream)
            writer.writerow(("time_s",) + CHANNEL_NAMES)
            for timestamp, values in self.rows:
                writer.writerow((f"{timestamp:.6f}",) + values)
