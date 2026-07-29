import os
import unittest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication

from mspm0_configurator.plot_widget import TelemetryPlot


class TelemetryPlotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_render_decimation_preserves_full_export_buffer(self):
        plot = TelemetryPlot()
        plot.resize(640, 400)
        plot.show()
        self.app.processEvents()
        values = tuple(float(index) for index in range(11))
        for index in range(10_000):
            row = (index / 1_000.0, values)
            plot.rows.append(row)
            plot.visible_rows.append(row)
        plot._redraw(force=True)
        self.assertEqual(len(plot.rows), 10_000)
        self.assertLessEqual(plot.series[0].count(), plot.view.viewport().width() * 2 + 1)
        self.assertGreaterEqual(plot.redraw_interval_s, 1.0 / 60.0)
        self.assertLessEqual(plot.redraw_interval_s, 1.0 / 10.0)
        plot.close()


if __name__ == "__main__":
    unittest.main()
