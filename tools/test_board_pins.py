#!/usr/bin/env python3
import copy
import json
import unittest
from pathlib import Path

from board_pins import check

ROOT = Path(__file__).resolve().parents[1]


def load():
    return json.loads((ROOT / "config/board_pins.json").read_text(encoding="utf-8"))


class BoardPinGateTest(unittest.TestCase):
    def test_checked_in_config_passes_and_is_unverified(self):
        cfg = load()
        self.assertEqual(check(cfg), [])
        self.assertFalse(cfg["verified"])

    def test_example_template_passes(self):
        cfg = json.loads((ROOT / "config/board_pins.example.json").read_text(encoding="utf-8"))
        self.assertEqual(check(cfg), [])

    def test_lcd_cs_matches_schematic(self):
        self.assertEqual(load()["display"]["pins"]["cs"]["gpio"], 15)

    def test_audio_pins_rejected_without_wiring_evidence(self):
        cfg = copy.deepcopy(load())
        del cfg["audio"]["wiring_evidence"]
        self.assertTrue(any("H03" in e for e in check(cfg)))
        cfg = json.loads((ROOT / "config/board_pins.example.json").read_text(encoding="utf-8"))
        cfg["audio"]["pins"]["bclk"]["gpio"] = 4
        self.assertTrue(any("H03" in e for e in check(cfg)))

    def test_user_reported_audio_wiring_is_not_bench_verified(self):
        cfg = load()
        self.assertEqual(cfg["audio"]["wiring_evidence"]["source"], "user_reported")
        self.assertEqual(cfg["audio"]["wiring_evidence"]["bench_tests"], [])
        pins = {k: v["gpio"] for k, v in cfg["audio"]["pins"].items()}
        self.assertEqual(pins, {"bclk": 4, "ws": 5, "mic_data_in": 6, "amp_data_out": 7})

    def test_blank_wiring_description_rejected(self):
        cfg = copy.deepcopy(load())
        cfg["audio"]["wiring_evidence"]["description"] = " "
        self.assertTrue(any("H03" in e for e in check(cfg)))

    def test_duplicate_gpio_rejected(self):
        cfg = load()
        cfg["touch"]["pins"]["penirq"]["gpio"] = 15
        self.assertTrue(any("already assigned" in e for e in check(cfg)))

    def test_strapping_pin_rejected_for_peripheral(self):
        cfg = load()
        cfg["touch"]["pins"]["penirq"]["gpio"] = 45
        self.assertTrue(any("strapping" in e for e in check(cfg)))

    def test_verified_requires_bench_evidence(self):
        cfg = copy.deepcopy(load())
        cfg["verified"] = True
        errors = check(cfg)
        self.assertTrue(any("bench" in e for e in errors))
        self.assertTrue(any("physical_pcb_revision" in e for e in errors))

    def test_audio_pin_outside_pool_rejected(self):
        cfg = load()
        cfg.update(verified=True, verification_level="bench", physical_pcb_revision="v1.0", bench_tests=["x"])
        cfg["audio"]["pins"]["ws"]["gpio"] = 8
        self.assertTrue(any("candidate_pool" in e for e in check(cfg)))


if __name__ == "__main__":
    unittest.main()
