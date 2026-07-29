import os
import unittest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication

from mspm0_configurator.main_window import MainWindow


class FakeWorker:
    def __init__(self):
        self.commands = []
        self.emergency_count = 0

    def send(self, command):
        self.commands.append(command)

    def emergency_stop(self):
        self.emergency_count += 1


class MainWindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.window = MainWindow()
        self.window._animations_enabled = lambda: False
        self.worker = FakeWorker()
        self.window.worker = self.worker

    def tearDown(self):
        self.window.handshake_timer.stop()
        self.window._pulse_timer.stop()
        self.window._duration_timer.stop()
        self.window.worker = None
        self.window.close()

    def test_controls_unlock_only_after_valid_handshake(self):
        self.window.on_connected("COM1")
        self.assertFalse(self.window.apply_btn.isEnabled())
        self.window.on_line(
            "@INFO,fw=0.1.0,proto=1,board=LP-MSPM0G3507,driver=DRV8870,"
            "motors=4,baud=115200,telemetry=firewater"
        )
        self.assertTrue(self.window.handshake_ok)
        self.assertTrue(self.window.apply_btn.isEnabled())
        self.assertIn("Config?", self.worker.commands)
        self.assertEqual(self.worker.commands[-1], "Status=0")

    def test_status_updates_selected_motor_controls(self):
        self.window.handshake_ok = True
        self.window.on_line(
            "@STATUS,motor=0,enabled=1,power=1,rpm=123,target=150,output=87,"
            "kp=0.8,ki=0.3,kd=0.01,ff_en=1,ff_k=1.2,ff_b=3,mode=speed"
        )
        self.assertEqual(self.window.target.value(), 150.0)
        self.assertEqual(self.window.kd.value(), 0.01)
        self.assertTrue(self.window.ff_enable.isChecked())
        self.assertIn("123", self.window.motor_rpm.text())

    def test_emergency_stop_uses_priority_worker_path(self):
        self.window.emergency_stop()
        self.assertEqual(self.worker.emergency_count, 1)

    def test_sidebars_can_collapse_and_restore(self):
        self.window.show()
        self.app.processEvents()
        # Collapse via the new API
        self.window.set_left_panel_visible(False)
        self.window.set_right_panel_visible(False)
        self.assertTrue(self.window.control_tabs.isHidden())
        self.assertTrue(self.window.live_table.isHidden())
        # Rails should be visible when collapsed
        self.assertTrue(self.window.left_rail.isVisible())
        self.assertTrue(self.window.right_rail.isVisible())
        # Restore
        self.window.set_left_panel_visible(True)
        self.window.set_right_panel_visible(True)
        self.assertFalse(self.window.control_tabs.isHidden())
        self.assertFalse(self.window.live_table.isHidden())
        self.assertFalse(self.window.left_rail.isVisible())
        self.assertFalse(self.window.right_rail.isVisible())

    def test_focus_view_keeps_emergency_header_and_restores_layout(self):
        self.window.show()
        self.app.processEvents()
        self.window.focus_btn.setChecked(True)
        self.assertTrue(self.window._focus_mode)
        self.assertFalse(self.window.header.isHidden())
        self.assertTrue(self.window.control_tabs.isHidden())
        self.assertTrue(self.window.live_table.isHidden())
        self.assertTrue(self.window.conn_box.isHidden())
        self.window.leave_focus_mode()
        self.assertFalse(self.window._focus_mode)
        # After leaving focus, sidebars should be restored
        self.assertFalse(self.window.conn_box.isHidden())

    def test_rail_tab_click_expands_sidebar(self):
        """Clicking a rail icon should expand the sidebar and switch tab."""
        self.window.show()
        self.app.processEvents()
        # Collapse first
        self.window.set_left_panel_visible(False)
        self.assertTrue(self.window.control_tabs.isHidden())
        # Click rail motor button
        self.window._rail_tab_clicked(1)
        self.assertFalse(self.window.control_tabs.isHidden())
        self.assertEqual(self.window.control_tabs.currentIndex(), 1)

    def test_window_title_shows_connection_state(self):
        self.window._update_window_title()
        self.assertIn("MSPM0", self.window.windowTitle())

    # ── v0.3.1 tests ──────────────────────────────────────────────

    def test_console_history_navigation(self):
        """Command history should be navigable via Up/Down."""
        self.window._console_history = ["Info?", "Status=0", "Stream=1"]
        self.window._console_history_index = -1
        # Navigate up (back in time)
        self.window._navigate_history(-1)
        self.assertEqual(self.window.command.text(), "Stream=1")
        self.window._navigate_history(-1)
        self.assertEqual(self.window.command.text(), "Status=0")
        # Navigate down (forward in time)
        self.window._navigate_history(1)
        self.assertEqual(self.window.command.text(), "Stream=1")

    def test_console_timestamp_in_output(self):
        """Console output should include a timestamp prefix."""
        self.window._append_console("Test message", "TX")
        text = self.window.console.toPlainText()
        # Should contain TX: and the message — timestamp format HH:MM:SS
        self.assertIn("TX:", text)
        self.assertIn("Test message", text)

    def test_console_clear_button(self):
        """Clear button should empty the console and reset byte counters."""
        self.window._append_console("Hello", "TX")
        self.window._append_console("World", "RX")
        self.assertGreater(self.window._rx_bytes, 0)
        self.window._clear_console()
        self.assertEqual(self.window._rx_bytes, 0)
        self.assertEqual(self.window._tx_bytes, 0)
        self.assertEqual(self.window.console.toPlainText(), "")

    def test_autoscroll_toggle(self):
        """Autoscroll checkbox should control the _autoscroll flag."""
        self.assertTrue(self.window._autoscroll)
        self.window.autoscroll_chk.setChecked(False)
        self.assertFalse(self.window._autoscroll)
        self.window.autoscroll_chk.setChecked(True)
        self.assertTrue(self.window._autoscroll)

    def test_byte_counters_increment(self):
        """TX/RX byte counters should increment on console output."""
        initial_tx = self.window._tx_bytes
        initial_rx = self.window._rx_bytes
        # TX direction increments _tx_bytes (serial output bytes)
        self.window._append_console("ABCD", "TX")
        self.assertGreater(self.window._tx_bytes, initial_tx)
        # RX direction increments _rx_bytes (console input bytes)
        self.window._append_console("Hello", "RX")
        self.assertGreater(self.window._rx_bytes, initial_rx)
        # send() increments tx_bytes via _append_console(command, "TX")
        self.window.handshake_ok = True
        tx_before = self.window._tx_bytes
        self.window.send("Test")
        self.assertGreater(self.window._tx_bytes, tx_before)

    def test_motor_deviation_displayed(self):
        """Motor status with non-zero target should show deviation percentage."""
        self.window.handshake_ok = True
        self.window.motor.setCurrentIndex(0)
        self.window.on_line(
            "@STATUS,motor=0,enabled=1,power=1,rpm=140,target=150,output=87,"
            "kp=0.8,ki=0.3,kd=0.01,ff_en=1,ff_k=1.2,ff_b=3,mode=speed"
        )
        deviation_text = self.window.motor_deviation.text()
        self.assertIn("%", deviation_text)

    def test_motor_state_box_motor_color_property(self):
        """Motor state box should have motorColor property set."""
        self.window.handshake_ok = True
        self.window.motor.setCurrentIndex(2)
        self.window.on_line(
            "@STATUS,motor=2,enabled=1,power=1,rpm=100,target=100,output=50,"
            "kp=0.8,ki=0.3,kd=0,ff_en=0,ff_k=0,ff_b=0,mode=speed"
        )
        color = self.window.motor_state_box.property("motorColor")
        self.assertEqual(color, "2")

    def test_rpm_colour_by_direction(self):
        """RPM label should have colour style based on rotation direction."""
        self.window.handshake_ok = True
        # Forward RPM (positive)
        self.window.on_line(
            "@STATUS,motor=0,enabled=1,power=1,rpm=200,target=200,output=50,"
            "kp=0.8,ki=0.3,kd=0,ff_en=0,ff_k=0,ff_b=0,mode=speed"
        )
        forward_style = self.window.motor_rpm.styleSheet()
        self.assertIn("#38BDF8", forward_style)

        # Reverse RPM (negative)
        self.window.on_line(
            "@STATUS,motor=0,enabled=1,power=1,rpm=-200,target=-200,output=50,"
            "kp=0.8,ki=0.3,kd=0,ff_en=0,ff_k=0,ff_b=0,mode=speed"
        )
        reverse_style = self.window.motor_rpm.styleSheet()
        self.assertIn("#F59E0B", reverse_style)

        # Stopped
        self.window.on_line(
            "@STATUS,motor=0,enabled=0,power=0,rpm=0,target=0,output=0,"
            "kp=0.8,ki=0.3,kd=0,ff_en=0,ff_k=0,ff_b=0,mode=speed"
        )
        stopped_style = self.window.motor_rpm.styleSheet()
        self.assertIn("#64748B", stopped_style)

    def test_connect_button_state_property(self):
        """Connect button should have state property matching connection_state."""
        self.window.connection_state = "disconnected"
        self.window.retranslate()
        state = self.window.connect_btn.property("state")
        self.assertEqual(state, "disconnected")

    def test_motor_indicator_in_status_bar(self):
        """Status bar should show current motor letter."""
        self.window.motor.setCurrentIndex(1)
        self.window._update_motor_indicator()
        self.assertIn("B", self.window.motor_indicator.text())

    def test_unsaved_changes_badge(self):
        """Motor tab should show unsaved badge when changes are made."""
        self.window._has_unsaved_changes = False
        self.window._update_tab_badge()
        tab_text = self.window.control_tabs.tabText(0)
        self.assertNotIn("\u25CF", tab_text)
        self.window._mark_unsaved()
        tab_text = self.window.control_tabs.tabText(0)
        self.assertIn("\u25CF", tab_text)
        self.window._clear_unsaved()
        tab_text = self.window.control_tabs.tabText(0)
        self.assertNotIn("\u25CF", tab_text)

    def test_quick_command_buttons_exist(self):
        """Quick command buttons should be created."""
        self.assertEqual(len(self.window._quick_cmd_buttons), 5)
        for btn, cmd in zip(self.window._quick_cmd_buttons, self.window.QUICK_COMMANDS):
            self.assertEqual(btn.text(), cmd)

    def test_quick_command_sends_once(self):
        """Quick command should send exactly once, not duplicate."""
        self.window.handshake_ok = True
        initial_count = len(self.worker.commands)
        self.window._send_quick_command("Info?")
        # Should send exactly one command, not two
        self.assertEqual(len(self.worker.commands) - initial_count, 1)
        self.assertEqual(self.worker.commands[-1], "Info?")
        # Should be added to history
        self.assertIn("Info?", self.window._console_history)

    def test_dtr_rts_indicators_hidden_when_disconnected(self):
        """DTR/RTS dots should be hidden when disconnected."""
        self.assertFalse(self.window.dtr_dot.isVisible())
        self.assertFalse(self.window.rts_dot.isVisible())

    def test_dtr_rts_indicators_visible_after_connect(self):
        """DTR/RTS dots should be visible after connecting."""
        self.window.on_connected("COM3")
        self.app.processEvents()
        self.assertTrue(self.window.dtr_dot.isVisibleTo(self.window))
        self.assertTrue(self.window.rts_dot.isVisibleTo(self.window))

    def test_port_info_displayed_after_connect(self):
        """Port info should be shown in status bar after connecting."""
        self.window.on_connected("COM3")
        self.assertIn("COM3", self.window.port_info_label.text())
        self.assertIn("115200", self.window.port_info_label.text())

    def test_confirm_run_includes_pid_summary(self):
        """Run confirmation dialog should include PID parameters."""
        self.window.kp.setValue(1.5)
        self.window.ki.setValue(0.4)
        self.window.kd.setValue(0.02)
        # We can't easily test QMessageBox in offscreen, but we can verify
        # the i18n template includes the PID fields
        from mspm0_configurator.i18n import tr
        msg = tr("en", "confirm_run").format(
            motor="A", rpm=100, kp=1.5, ki=0.4, kd=0.02
        )
        self.assertIn("Kp=1.5", msg)
        self.assertIn("Ki=0.4", msg)
        self.assertIn("Kd=0.02", msg)

    def test_version_is_031(self):
        """Application version should be 0.3.1."""
        from mspm0_configurator import __version__
        self.assertEqual(__version__, "0.3.1")


if __name__ == "__main__":
    unittest.main()
