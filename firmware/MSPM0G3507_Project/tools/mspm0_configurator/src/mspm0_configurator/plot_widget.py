from __future__ import annotations

import csv
import math
import time
from collections import deque
from pathlib import Path

from PySide6.QtCharts import QChart, QChartView, QLineSeries, QValueAxis
from PySide6.QtCore import QPointF, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QVBoxLayout, QWidget

from .protocol import CHANNEL_NAMES, TelemetryFrame

# Color-blind-conscious palette; line styles also distinguish channel groups.
COLORS = (
    "#38BDF8", "#F59E0B", "#A78BFA", "#34D399",
    "#7DD3FC", "#FCD34D", "#C4B5FD", "#6EE7B7",
    "#FB7185", "#F472B6", "#E2E8F0",
)


class TelemetryPlot(QWidget):
    """Bounded, throttled plot for all protocol-v1 telemetry channels."""

    frame_updated = Signal(object, float)
    render_rate_updated = Signal(float)
    MIN_REDRAW_INTERVAL_S = 1.0 / 60.0
    MAX_REDRAW_INTERVAL_S = 0.10
    RATE_WINDOW = 60

    def __init__(self, parent=None):
        super().__init__(parent)
        self.window_s = 15.0
        self.paused = False
        self.started = time.monotonic()
        self.rows: deque[tuple[float, tuple[float, ...]]] = deque(maxlen=20_000)
        self.visible_rows: deque[tuple[float, tuple[float, ...]]] = deque()
        self.arrivals: deque[float] = deque(maxlen=self.RATE_WINDOW)
        self.last_redraw = 0.0
        self.redraw_interval_s = self.MIN_REDRAW_INTERVAL_S
        self.render_times: deque[float] = deque(maxlen=30)

        self.chart = QChart()
        self.chart.setBackgroundBrush(QColor("#0F172A"))
        self.chart.setPlotAreaBackgroundBrush(QColor("#0B1220"))
        self.chart.setPlotAreaBackgroundVisible(True)
        self.chart.legend().setVisible(True)
        self.chart.legend().setLabelColor(QColor("#CBD5E1"))
        self.chart.setAnimationOptions(QChart.AnimationOption.NoAnimation)

        self.axis_x = QValueAxis()
        self.axis_x.setTitleText("Time · s")
        self.axis_y = QValueAxis()
        self.axis_y.setTitleText("Value")
        for axis in (self.axis_x, self.axis_y):
            axis.setLabelsColor(QColor("#94A3B8"))
            axis.setTitleBrush(QColor("#CBD5E1"))
            axis.setGridLineColor(QColor("#243247"))
        self.chart.addAxis(self.axis_x, Qt.AlignmentFlag.AlignBottom)
        self.chart.addAxis(self.axis_y, Qt.AlignmentFlag.AlignLeft)
        self.axis_x.setRange(0.0, self.window_s)
        self.axis_y.setRange(-10.0, 10.0)

        self.series: list[QLineSeries] = []
        for index, name in enumerate(CHANNEL_NAMES):
            series = QLineSeries()
            series.setName(name)
            pen = QPen(QColor(COLORS[index]), 2.0)
            if 4 <= index < 8:
                pen.setStyle(Qt.PenStyle.DashLine)
            elif index >= 8:
                pen.setStyle(Qt.PenStyle.DotLine)
            series.setPen(pen)
            self.chart.addSeries(series)
            series.attachAxis(self.axis_x)
            series.attachAxis(self.axis_y)
            self.series.append(series)

        self.set_group_visible("control", False)
        self.view = QChartView(self.chart)
        self.view.setRenderHint(QPainter.RenderHint.Antialiasing)
        self.view.setRubberBand(QChartView.RubberBand.RectangleRubberBand)
        self.view.setAccessibleName("Real-time motor telemetry chart")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.view)

    def set_window(self, seconds: int) -> None:
        self.window_s = float(seconds)
        self._trim_visible()
        self._redraw(force=True)

    def set_paused(self, paused: bool) -> None:
        self.paused = paused
        if not paused:
            self._redraw(force=True)

    def set_group_visible(self, group: str, visible: bool) -> None:
        ranges = {"rpm": range(0, 4), "target": range(4, 8), "control": range(8, 11)}
        for index in ranges[group]:
            self.series[index].setVisible(visible)
        if self.rows:
            self._redraw(force=True)

    def clear(self) -> None:
        self.rows.clear()
        self.visible_rows.clear()
        self.arrivals.clear()
        self.started = time.monotonic()
        self.last_redraw = 0.0
        self.redraw_interval_s = self.MIN_REDRAW_INTERVAL_S
        self.render_times.clear()
        for series in self.series:
            series.clear()
        self.axis_x.setRange(0.0, self.window_s)
        self.axis_y.setRange(-10.0, 10.0)

    def add_frame(self, frame: TelemetryFrame) -> None:
        now = time.monotonic()
        t = now - self.started
        row = (t, frame.values)
        self.rows.append(row)
        self.visible_rows.append(row)
        self.arrivals.append(now)
        self._trim_visible()

        rate = 0.0
        if len(self.arrivals) > 1:
            elapsed = self.arrivals[-1] - self.arrivals[0]
            if elapsed > 0:
                rate = (len(self.arrivals) - 1) / elapsed
        self.frame_updated.emit(frame.as_dict(), rate)
        if not self.paused:
            self._redraw()

    def _trim_visible(self) -> None:
        if not self.visible_rows:
            return
        cutoff = self.visible_rows[-1][0] - self.window_s
        while self.visible_rows and self.visible_rows[0][0] < cutoff:
            self.visible_rows.popleft()

    def _redraw(self, force: bool = False) -> None:
        now = time.monotonic()
        if not force and now - self.last_redraw < self.redraw_interval_s:
            return
        self.last_redraw = now
        if not self.visible_rows:
            return

        render_started = time.monotonic()
        rows = list(self.visible_rows)
        # Bound paint work to the available pixels. Serial samples remain intact
        # in ``rows`` for CSV export while the screen receives at most two
        # points per horizontal pixel.
        point_budget = max(500, self.view.viewport().width() * 2)
        stride = max(1, math.ceil(len(rows) / point_budget))
        sampled = rows[::stride]
        if sampled[-1] is not rows[-1]:
            sampled.append(rows[-1])
        self.view.setRenderHint(
            QPainter.RenderHint.Antialiasing, len(sampled) <= 4_000
        )

        for index, series in enumerate(self.series):
            if series.isVisible():
                series.replace(
                    [QPointF(x, values[index]) for x, values in sampled]
                )

        latest_t = self.visible_rows[-1][0]
        cutoff = latest_t - self.window_s
        self.axis_x.setRange(max(0.0, cutoff), max(self.window_s, latest_t))
        visible_indices = [i for i, series in enumerate(self.series) if series.isVisible()]
        values_y = [
            values[index]
            for _, values in sampled
            for index in visible_indices
        ]
        if values_y:
            low, high = min(values_y), max(values_y)
            padding = max(10.0, (high - low) * 0.1)
            self.axis_y.setRange(low - padding, high + padding)

        rendered_at = time.monotonic()
        render_cost = rendered_at - render_started
        self.redraw_interval_s = min(
            self.MAX_REDRAW_INTERVAL_S,
            max(self.MIN_REDRAW_INTERVAL_S, render_cost * 2.5),
        )
        self.render_times.append(rendered_at)
        render_rate = 0.0
        if len(self.render_times) > 1:
            elapsed = self.render_times[-1] - self.render_times[0]
            if elapsed > 0:
                render_rate = (len(self.render_times) - 1) / elapsed
        self.render_rate_updated.emit(render_rate)

    def export_csv(self, path: str | Path) -> None:
        with Path(path).open("w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.writer(stream)
            writer.writerow(("time_s",) + CHANNEL_NAMES)
            for timestamp, values in self.rows:
                writer.writerow((f"{timestamp:.6f}",) + values)
