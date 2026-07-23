import unittest

from mspm0_configurator.protocol import (
    CommandBuilder,
    LineKind,
    decode_line,
    encode_command,
)


class ProtocolTests(unittest.TestCase):
    def test_telemetry(self):
        decoded = decode_line(",".join(str(index) for index in range(11)))
        self.assertEqual(decoded.kind, LineKind.TELEMETRY)
        self.assertEqual(decoded.telemetry.as_dict()["selected_output"], 10.0)

    def test_machine(self):
        decoded = decode_line("@INFO,fw=0.1.0,proto=1")
        self.assertEqual(decoded.kind, LineKind.MACHINE)
        self.assertEqual(decoded.machine.fields["fw"], "0.1.0")

    def test_text(self):
        self.assertEqual(decode_line("All stopped").kind, LineKind.TEXT)
        self.assertEqual(decode_line("@").kind, LineKind.TEXT)

    def test_commands(self):
        self.assertEqual(CommandBuilder.info_query(), "Info?")
        self.assertEqual(CommandBuilder.config_query(), "Config?")
        self.assertEqual(CommandBuilder.status(), "Status?")
        self.assertEqual(CommandBuilder.status(2), "Status=2")
        self.assertEqual(CommandBuilder.stream(True), "Stream=1")
        self.assertEqual(CommandBuilder.stream(False), "Stream=0")
        self.assertEqual(CommandBuilder.pid("kp", 1.25), "Kp=1.25")
        self.assertEqual(CommandBuilder.run(3), "Run=3")
        self.assertEqual(CommandBuilder.stop_all(), "StopAll")
        self.assertEqual(CommandBuilder.angle(90, 100), "angle=90,100")

    def test_rejects_unsafe_values(self):
        with self.assertRaises(ValueError):
            CommandBuilder.target(801)
        with self.assertRaises(ValueError):
            CommandBuilder.run(4)
        with self.assertRaises(ValueError):
            CommandBuilder.pid("bad", 1)
        with self.assertRaises(ValueError):
            CommandBuilder.run(1.5)
        with self.assertRaises(ValueError):
            CommandBuilder.run(True)
        with self.assertRaises(ValueError):
            CommandBuilder.position(1000, 301)

    def test_command_line_encoding(self):
        self.assertEqual(encode_command(" StopAll "), b"StopAll\r\n")
        with self.assertRaises(ValueError):
            encode_command("")
        with self.assertRaises(ValueError):
            encode_command("StopAll\nRun=0")
        with self.assertRaises(UnicodeEncodeError):
            encode_command("停止")


if __name__ == "__main__":
    unittest.main()
