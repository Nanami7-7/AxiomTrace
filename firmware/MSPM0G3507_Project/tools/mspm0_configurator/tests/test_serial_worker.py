import unittest

from mspm0_configurator.serial_worker import SerialWorker


class FakeSerial:
    def __init__(self):
        self.writes = []

    def write(self, payload):
        self.writes.append(payload)


class SerialWorkerTests(unittest.TestCase):
    def test_emergency_stop_discards_normal_commands_and_runs_first(self):
        worker = SerialWorker("TEST", 115200)
        worker.send("Target=400")
        worker.send("Run=0")
        worker.emergency_stop()
        serial = FakeSerial()
        worker._drain_commands(serial)
        self.assertEqual(serial.writes, [b"StopAll\r\n", b"Stream=0\r\n"])

    def test_shutdown_flushes_only_final_safety_commands(self):
        worker = SerialWorker("TEST", 115200)
        worker.send("Run=0")
        worker.shutdown(("StopAll", "Stream=0"))
        serial = FakeSerial()
        worker._drain_commands(serial)
        self.assertEqual(serial.writes, [b"StopAll\r\n", b"Stream=0\r\n"])


if __name__ == "__main__":
    unittest.main()
