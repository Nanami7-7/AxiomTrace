import json
import tempfile
import unittest
from pathlib import Path

from mspm0_configurator.settings import AppSettings


class SettingsTests(unittest.TestCase):
    def test_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            AppSettings(language="en", motor=2, kp=1.2).save(path)
            loaded = AppSettings.load(path)
            self.assertEqual(loaded.language, "en")
            self.assertEqual(loaded.motor, 2)
            self.assertEqual(loaded.kp, 1.2)

    def test_rejects_invalid_motor(self):
        with self.assertRaises(ValueError):
            AppSettings(motor=4)

    def test_rejects_non_object_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            path.write_text(json.dumps([1, 2, 3]), encoding="utf-8")
            with self.assertRaises(ValueError):
                AppSettings.load(path)


if __name__ == "__main__":
    unittest.main()
