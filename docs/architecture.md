# Historical v1 architecture and interfaces

This document describes the former v1 implementation, which has been removed.
The active v2 architecture and contracts are in
[`v2/docs/05_architecture.md`](../v2/docs/05_architecture.md).
## Stack
ESP-IDF (version chosen in compatibility spike), C++17, LVGL (compatible version), ST7789 SPI display driver, NS2009 touch driver, ESP-IDF I2S standard driver, ESP-SR WakeNet/MultiNet if validated. FreeRTOS queues. Prefer ESP-IDF component manager with lockfile. No Arduino-only assumption.

## Components
- `board`: display init, touch sampling/calibration, backlight, pin verification, SD optional
- `ui`: LVGL owner task, 320×240 landscape, navigation and focus/large hit areas
- `audio`: mixer/serialized effects and speech playback; rate/volume limiter; I2S TX
- `speech`: I2S RX, optional AFE/VAD, WakeNet/MultiNet, grammar, command events, confidence handling
- `game_core`: game registry, scene lifecycle, deterministic RNG, event bus, idle timers
- `games/*`: individual scenes, asset IDs and game logic
- The former Color Pop PixelRoot32 renderer/APU was removed with the v1 implementation.
- `assets`: compressed images and speech/audio manifests; integrity checks
- `settings`: parent gate, capped volume, speech on/off, calibration, no cloud

## Contract (normative pseudocode; implement headers in first platform PR)
```
enum class Intent { START, BACK, AGAIN, YES, NO, RED, BLUE, GREEN, YELLOW,
                    ONE, TWO, THREE, GO, STOP, HELP, NEXT, PAUSE, RESUME,
                    ROBOT, COLORS, EGGS, MUSIC, SHAPES, ANIMALS, SPACE,
                    FACES, COUNTING, FISH, UNKNOWN };
struct InputEvent { Source source; Intent intent; int16_t x, y; uint32_t timestamp_ms; };
class IGame {
 public: virtual ~IGame() = default;
 virtual void enter(GameContext&) = 0;
 virtual void update(uint32_t dt_ms) = 0;
 virtual void handle(const InputEvent&) = 0;
 virtual void exit() = 0;
};
```
Only the UI task calls LVGL; voice task sends typed intents to a bounded queue. All assets are loaded by ID; games do not allocate large persistent buffers. Game input priority: physical parent menu > navigation > voice/touch game actions. Debounce repeated voice intents and touch taps; no indefinite blocking.

## Screen/UI
320×240 landscape; large touch targets >= 48×48 px preferred, >= 8 px spacing, max 2–4 simultaneous choices for youngest players. Keep navigation fixed and obvious. Resistive single-touch only; no multitouch, fast swipe, or precise drag required. Provide large tap-to-select alternatives for shape placement.

## Memory/flash
16 MB flash is not 16 MB usable assets. Measure actual binary, model and filesystem partition consumption. Start with single OTA-less app partition for prototype; decide OTA vs asset partition later. Do not adopt vendor Arduino 3 MB app partition unchanged. Set budget gates: app+models <= chosen app partition; minimum runtime free internal heap >= 64 KiB and PSRAM >= 1 MiB after all tasks/models initialized (provisional, tune from evidence). Use heap tracing and watermark logs. Reserve DMA-capable internal RAM for SPI/I2S buffers. Display framebuffer RGB565 full frame = 153,600 bytes; prefer partial LVGL draw buffers, sized empirically.

## Audio concurrency
Speech recognizer and sound playback may overlap: duck or briefly pause effects during listening; prevent self-triggering by echo suppression or a listen window. Use 16 kHz mono capture as initial hypothesis, verify ESP-SR model AFE requirements. TTS is prerecorded short prompts; sound effects may be synthesized. Don't claim full duplex until tested.
