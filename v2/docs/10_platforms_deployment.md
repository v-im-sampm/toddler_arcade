# 10: Platforms, build and deployment

Concrete commands are written after ADR-009/010/011. This document fixes the **requirements** for
each pipeline.

## 1. ESP32-S3 device
- **Toolchain:** ESP-IDF (pin the newest stable release that the chosen stack and ESP-SR support;
  v1 used v5.5.6) inside a **WSL container (`wslc`) image**, never Docker. Evolve
  `tools/idf_wslc_build.ps1` (prepare/build/clean/rebuild, size reports, host tests).
- **Flashing:** a Windows-host script with pinned esptool (v1 `tools/flash_firmware.ps1`). It writes
  the bootloader, partition table, app, model and asset partitions, and verifies hashes.
- **Partitions (ADR-011):** start from v1 (4 MB factory app, 6 MB model, ≈ 5.9 MB assets). Re-size
  from measured app, model and asset totals plus ≥ 10 % headroom.
  - OTA over Wi-Fi is **out of scope** (no network).
  - Optional updates come by USB flashing or a parent-provided SD card containing a signed asset/app
    bundle. Decide in ADR-011, with integrity checks and rollback.
- **Boot:** display first frame ≤ 1 s (splash = launcher island, no sound), launcher interactive
  ≤ 3 s. Audio clocks start only after the engine writes silence (pop avoidance).
- **Device bench mode:** a serial command interface (USB-CDC/UART) to start a replay and dump
  metrics: FPS histogram, heap/PSRAM minima, audio CPU, and touch latency (with an optional GPIO
  marker or a photodiode/mic method documented in `11`).
- **Factory reset:** a parent area option + documented NVS erase command (v1 parity).

## 2. Web (PWA)
- **Build** produces a static bundle:
  - HTML shell, engine (WASM and/or JS), per-game lazy-loaded packs, service worker, and manifest
    icons;
  - no CDN, fonts or analytics from third-party origins.
- **Security headers / CSP:**
  `default-src 'self'; connect-src 'self'; img-src 'self' data: blob:; media-src 'self' blob:;
  script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; frame-ancestors 'none'`. Adjust only with
  a justification. If threads are needed, add COOP/COEP for `SharedArrayBuffer`.
- **Offline:**
  - precache the shell plus the launcher;
  - cache games on first play, with a parent option to "download all for offline";
  - versioned caches, with an update applied only on the next cold start (never mid-play).
- **Audio:** unlock the AudioContext on the first user gesture (invisible to the child). Use an
  AudioWorklet for the mixer if the engine renders its own audio.
- **Input:** Pointer Events. Block browser gestures (pinch-zoom, pull-to-refresh, long-press menu,
  text selection) inside the stage.
- **Hosting:** any static host over HTTPS. No server logic. No cookies.
- **Fullscreen/orientation:** request fullscreen on a parent action. Show the "turn me" animation in
  portrait.

## 3. Phone (Android + iOS)
- Packaging per ADR-009 (PWA-only, wrapper, or native). Requirements regardless of choice:
  - No INTERNET permission on Android if technically achievable (wrapper libraries may add it; audit
    the merged manifest).
  - On iOS, no networking entitlements used.
  - Only the RECORD_AUDIO / microphone permission, requested only after the parent enables voice or
    recording.
  - Landscape lock, immersive/fullscreen, keep-screen-on during play, and respect for the system
    silent mode/volume.
  - Kids-store compliance: research and cite the current **Google Play Families Policy**, **Apple
    App Store Review Guidelines (Kids Category, §1.3 and §5.1.4)**, and **COPPA**. A parental gate
    is required before any external link (there are none) or permission prompt.
  - Parent guidance screen describing Android screen pinning / iOS Guided Access (text in the parent
    area).
  - The privacy policy is a static page stating that no data is collected (required by stores),
    bundled in the app and hosted statically.
- CI produces debug builds. Signing/release is an owner-run step with documented commands.

## 4. Shared release process
1. Version: `v2.MINOR.PATCH`. The same version is shown in the parent About screen on all platforms.
2. Release checklist (`11_qa_acceptance.md` gates) with evidence attached per platform.
3. Artifacts:
   - device firmware bundle (all partitions + flash script + checksums);
   - web static bundle (zip + hash);
   - Android AAB/APK, iOS archive (owner-signed);
   - licence/notice file;
   - release notes with known limitations.
4. Rollback:
   - device: previous bundle + flash script;
   - web: previous bundle redeploy (the service worker handles the version swap on the next cold
     start);
   - stores: the standard staged rollout.
