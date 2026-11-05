"""Builds the Animal Sounds (G06) call clips from public-domain/CC0 recordings.

Dev-time only (the firmware never touches the network). For each animal in
assets/audio/animal_sounds/sources.json this downloads the listed recording
into a local cache, verifies its SHA-256 (recorded on first run), decodes a
window with ffmpeg, finds the loudest call, trims it, band-limits it for the
small speaker, normalises it to a conservative peak, fades the edges and
writes a canonical 16 kHz mono 16-bit WAV next to sources.json. The firmware
build turns those WAVs into flash tables (firmware/components/audio/animal_clips.cmake).

Usage: python tools/make_animal_clips.py [--update-hashes]
Requires ffmpeg on PATH.
"""

from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import wave
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CLIP_DIR = REPO / "assets" / "audio" / "animal_sounds"
SOURCES = CLIP_DIR / "sources.json"
ANIMALS = ["cow", "duck", "sheep", "pig", "dog", "cat", "frog", "owl"]
ALLOWED_LICENCES = ("cc0", "public domain")

RATE = 16000
FRAME = RATE // 100  # 10 ms analysis frames
PEAK = 0.88 * 32767  # firmware test caps clip data at 30000
MIN_S, DEFAULT_MAX_S = 0.3, 1.15  # firmware caps every sound at kMaxSoundMs (1200 ms)
FADE_IN_S, FADE_OUT_S = 0.010, 0.040
USER_AGENT = "LittleArcadeAssetBuild/1.0 (offline toddler toy; dev-time asset fetch)"


def fetch(url: str, cache: Path, referer: str | None = None) -> Path:
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / hashlib.sha256(url.encode()).hexdigest()[:16]
    if target.exists():
        return target
    partial = target.with_suffix(".part")
    # Some hosts reject Python's TLS client outright; curl (bundled with Windows 10+) is accepted.
    if shutil.which("curl"):
        cmd = ["curl", "-sSfL", "--max-time", "120", "-A", USER_AGENT, "-o", str(partial), url]
        if referer:
            cmd[1:1] = ["-e", referer]
        subprocess.run(cmd, check=True)
    else:
        headers = {"User-Agent": USER_AGENT, **({"Referer": referer} if referer else {})}
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=60) as resp, open(partial, "wb") as out:
            shutil.copyfileobj(resp, out)
    partial.replace(target)
    return target


def decode(path: Path, start_s: float, window_s: float) -> array.array:
    cmd = ["ffmpeg", "-v", "error", "-ss", f"{start_s}", "-t", f"{window_s}", "-i", str(path),
           "-ac", "1", "-ar", str(RATE), "-af", "highpass=f=150,lowpass=f=7000", "-f", "s16le", "-"]
    raw = subprocess.run(cmd, check=True, capture_output=True).stdout
    pcm = array.array("h")
    pcm.frombytes(raw)
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm


def frame_rms(pcm: array.array) -> list[float]:
    out = []
    for i in range(0, len(pcm) - FRAME + 1, FRAME):
        chunk = pcm[i:i + FRAME]
        out.append(math.sqrt(sum(v * v for v in chunk) / FRAME))
    return out


def find_call(pcm: array.array, max_s: float) -> tuple[int, int]:
    """Returns [start, end) samples around the loudest call, at most max_s long."""
    rms = frame_rms(pcm)
    if not rms:
        raise ValueError("decoded window is empty")
    loudest = max(range(len(rms)), key=rms.__getitem__)
    floor = sorted(rms)[len(rms) // 10]
    threshold = max(rms[loudest] * 0.08, floor * 3.0)
    max_frames = int(max_s * 100)
    # Walk outwards from the loudest frame while the call stays above threshold,
    # tolerating short dips (syllable gaps up to 120 ms).
    def walk(step: int) -> int:
        i, quiet, last_loud = loudest, 0, loudest
        while 0 <= i + step < len(rms) and abs(i + step - loudest) < max_frames:
            i += step
            if rms[i] >= threshold:
                last_loud, quiet = i, 0
            else:
                quiet += 1
                if quiet > 12:
                    break
        return last_loud

    first, last = walk(-1), walk(+1)
    start = max(0, first * FRAME - 3 * FRAME)
    end = min(len(pcm), (last + 1) * FRAME + 8 * FRAME)
    limit = int(max_s * RATE)
    if end - start > limit:  # keep the loudest part centred-ish, favouring the onset
        start = max(0, min(start, loudest * FRAME - limit // 3))
        end = start + limit
    minimum = int(MIN_S * RATE)
    if end - start < minimum:
        end = min(len(pcm), start + minimum)
    return start, end


def shape(pcm: array.array) -> array.array:
    top = max(1, max(abs(v) for v in pcm))
    gain = PEAK / top
    n = len(pcm)
    fade_in, fade_out = int(FADE_IN_S * RATE), int(FADE_OUT_S * RATE)
    out = array.array("h", [0] * n)
    for i, v in enumerate(pcm):
        env = 1.0
        if i < fade_in:
            env = 0.5 - 0.5 * math.cos(math.pi * i / fade_in)
        left = n - 1 - i
        if left < fade_out:
            env = min(env, 0.5 - 0.5 * math.cos(math.pi * left / fade_out))
        out[i] = int(round(max(-32767, min(32767, v * gain * env))))
    return out


def write_wav(path: Path, pcm: array.array) -> None:
    data = array.array("h", pcm)
    if sys.byteorder != "little":
        data.byteswap()
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--update-hashes", action="store_true", help="record SHA-256 of downloaded sources")
    parser.add_argument("--cache", type=Path, default=Path(tempfile.gettempdir()) / "little_arcade_clip_cache")
    args = parser.parse_args()
    if shutil.which("ffmpeg") is None:
        print("ffmpeg is required on PATH", file=sys.stderr)
        return 2

    sources = json.loads(SOURCES.read_text(encoding="utf-8"))
    changed = False
    for name in ANIMALS:
        src = sources["animals"][name]
        if not any(tag in src["license"].lower() for tag in ALLOWED_LICENCES):
            print(f"{name}: licence '{src['license']}' is not public domain/CC0", file=sys.stderr)
            return 1
        path = fetch(src["download_url"], args.cache, src.get("source_page"))
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if src.get("sha256") and src["sha256"] != digest:
            print(f"{name}: source changed (sha256 {digest} != {src['sha256']})", file=sys.stderr)
            return 1
        if src.get("sha256") != digest and args.update_hashes:
            src["sha256"] = digest
            changed = True
        max_s = float(src.get("max_s", DEFAULT_MAX_S))
        window_s = float(src.get("window_s", 8.0))
        pcm = decode(path, float(src.get("start_s", 0.0)), window_s)
        if src.get("exact"):  # hand-picked multi-syllable call ("woof woof"): keep the whole window
            if not MIN_S <= len(pcm) / RATE <= max_s + 0.01:
                print(f"{name}: exact window {len(pcm) / RATE:.2f} s outside {MIN_S}-{max_s} s", file=sys.stderr)
                return 1
            start, end = 0, len(pcm)
        else:
            start, end = find_call(pcm, max_s)
        clip = shape(pcm[start:end])
        write_wav(CLIP_DIR / f"{name}.wav", clip)
        rms = math.sqrt(sum(v * v for v in clip) / len(clip))
        offset = float(src.get("start_s", 0.0)) + start / RATE
        print(f"{name:6s} {len(clip) / RATE:5.2f} s from {offset:6.2f} s  peak {max(abs(v) for v in clip):5d}  rms {rms:6.0f}")
    if changed:
        SOURCES.write_text(json.dumps(sources, indent=2) + "\n", encoding="utf-8")
        print(f"recorded hashes in {SOURCES.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
