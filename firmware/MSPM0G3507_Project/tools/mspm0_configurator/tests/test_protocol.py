import unittest

from mspm0_configurator.protocol import (
    CommandBuilder,
    LineKind,
    decode_line,
    encode_command,
    parse_device_info,
    parse_motor_status,
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

    def test_typed_device_info(self):
        decoded = decode_line(
            "@INFO,fw=0.1.0,proto=1,board=LP-MSPM0G3507,driver=DRV8870,"
            "motors=4,baud=115200,telemetry=firewater"
        )
        info = parse_device_info(decoded.machine)
        self.assertEqual(info.protocol, 1)
        self.assertEqual(info.motors, 4)
        self.assertEqual(info.telemetry, "firewater")

    def test_typed_motor_status(self):
        decoded = decode_line(
            "@STATUS,motor=2,enabled=1,power=1,rpm=123,target=150,output=87,"
            "kp=0.8,ki=0.3,kd=0,ff_en=1,ff_k=1.2,ff_b=3,mode=speed"
        )
        status = parse_motor_status(decoded.machine)
        self.assertEqual(status.motor, 2)
        self.assertTrue(status.enabled)
        self.assertEqual(status.rpm, 123.0)
        self.assertEqual(status.ff_k, 1.2)

    def test_typed_messages_reject_incomplete_or_invalid_values(self):
        with self.assertRaises(ValueError):
            parse_device_info(decode_line("@INFO,fw=0.1.0").machine)
        with self.assertRaises(ValueError):
            parse_motor_status(
                decode_line(
                    "@STATUS,motor=0,enabled=2,power=1,rpm=0,target=0,output=0,"
                    "kp=0,ki=0,kd=0,ff_en=0,ff_k=0,ff_b=0,mode=speed"
                ).machine
            )

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
