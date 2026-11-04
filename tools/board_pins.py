#!/usr/bin/env python3
"""Pin-gate checks for config/board_pins*.json (H01 hardware rules)."""
import json
import sys
from pathlib import Path

FUNCTIONAL_GROUPS = ("display", "touch", "sd", "audio")
ESP32S3_MAX_GPIO = 48


def iter_pins(cfg):
    for group, body in cfg.items():
        if isinstance(body, dict) and isinstance(body.get("pins"), dict):
            for name, pin in body["pins"].items():
                yield group, name, pin["gpio"]


def check(cfg):
    errors = []
    if not isinstance(cfg.get("verified"), bool):
        errors.append("'verified' must be a boolean")
    if cfg.get("verified"):
        if cfg.get("verification_level") != "bench":
            errors.append("verified=true requires verification_level='bench'")
        if cfg.get("physical_pcb_revision", "TBD") == "TBD":
            errors.append("verified=true requires an observed physical_pcb_revision")
        if not cfg.get("bench_tests"):
            errors.append("verified=true requires recorded bench_tests evidence")

    blocked = set(cfg.get("strapping_or_reserved_gpios", []))
    blocked |= set(cfg.get("module_internal_gpios", []))
    blocked |= set(cfg.get("unrouted_gpios", {}).get("gpios", []))
    audio_pool = set(cfg.get("audio", {}).get("candidate_pool_gpios", []))

    seen = {}
    for group, name, gpio in iter_pins(cfg):
        label = f"{group}.{name}"
        if gpio == "TBD":
            continue
        if not isinstance(gpio, int) or isinstance(gpio, bool) or not 0 <= gpio <= ESP32S3_MAX_GPIO:
            errors.append(f"{label}: gpio must be an int 0..{ESP32S3_MAX_GPIO} or 'TBD', got {gpio!r}")
            continue
        if gpio in seen:
            errors.append(f"{label}: GPIO{gpio} already assigned to {seen[gpio]}")
        seen[gpio] = label
        if group in FUNCTIONAL_GROUPS and gpio in blocked:
            errors.append(f"{label}: GPIO{gpio} is strapping/reserved/internal/unrouted")
        if group == "audio":
            if not cfg.get("verified"):
                errors.append(f"{label}: audio pins stay TBD until H03 bench validation")
            if gpio not in audio_pool:
                errors.append(f"{label}: GPIO{gpio} is outside audio.candidate_pool_gpios")
    return errors


def check_file(path):
    return check(json.loads(Path(path).read_text(encoding="utf-8")))


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    paths = sys.argv[1:] or [root / "config/board_pins.json", root / "config/board_pins.example.json"]
    failed = False
    for p in paths:
        for e in check_file(p):
            failed = True
            print(f"FAIL {p}: {e}")
    if failed:
        sys.exit(1)
    print("PASS: board pin gate")
