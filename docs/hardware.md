# Hardware validation checklist — BLOCKER BEFORE WIRING
Source: https://github.com/Makerfabs/MaTouch_Lite_ESP32-S3-SPI-IPS-with-Touch-2.4-ST7789
Makerfabs README states ESP32-S3, 320×240 ST7789V SPI, NS2009 resistive touch, 16 MB flash, 8 MB PSRAM, microSD interface, 2× JST 1.25mm 4-pin expansion headers, USB-C power.

## H01 — source-reviewed pin map (NOT bench-validated)
Status: **source review only.** `config/board_pins.json` has `"verified": false`. No physical board was inspected, so the PCB revision printed on the board is `TBD`. H02/H03 must confirm every row on hardware before `verified` may become `true`.

Evidence:
- Schematic `MaTouch Lite ESP32-S3 SPI IPS with Touch 2.4'' ST7789 v1.0.PDF`, upstream commit `2632b6522ee5439ac8bfe9f038fb4d05006a8c57`, git blob `f3eed606b0160e0ff6f497a0e8cfc7df0121d2d3`. The PDF is not committed (upstream has no license file); fetch it from the URL in `config/board_pins.json`.
- Example `example/LVGL_Touch` at commit `630f37c09eeb1aa339a199f9e7b5f48eee76dab3`: `ui/ui.ino` (touch I2C pins, address, external I2C on J1) and `libraries/TFT_eSPI/User_Setup.h` (`TFT_MOSI 13`, `TFT_SCLK 14`, `TFT_CS 15`, `TFT_DC 21`, `TFT_RST 1`, `TFT_BL 48`, 27 MHz, BGR).

| Function | GPIO | Schematic net | Cross-check |
|---|---|---|---|
| LCD SCLK / MOSI / CS / DC / RST | 14 / 13 / 15 / 21 / 1 | `IO14/SCLK`, `IO13/MOSI`, `IO15/HNSS0`→CSX, `IO21/LCD_RS`→D/CX, `IO1/LCD_RST` | Matches example `User_Setup.h` |
| LCD SDO (read-back) | 12 | `IO12/HMISO` via diode D4 | Schematic only; example does not use MISO |
| Backlight | 48 | `IO48/BKL` → R10 → S8050 Q3 (low-side) | Matches example; active-high assumed |
| NS2009 SCL / SDA / PENIRQ | 9 / 10 / 11 | `IO9`, `IO10` (3.3 k pull-ups), `IO11` | SCL/SDA match example; PENIRQ unused by example |
| NS2009 address | `0x48` | A0 tied to GND | Matches example |
| TF card CLK / CMD / D0 | 42 / 2 / 41 | `IO42/SCLK`, `IO2/CMD`, `IO41/D0` | **1-bit SDIO**: DAT1, DAT2, CD/DAT3 only have 10 k pull-ups |
| J1 pins 1 / 2 | 4 / 5 | `IO4`, `IO5`; pin 3 = +3V3, pin 4 = GND | Example uses GPIO5 SDA / GPIO4 SCL for an external sensor |
| J2 pins 1 / 2 | 6 / 7 | `IO6`, `IO7`; pin 3 = +3V3, pin 4 = GND | — |
| USB native D− / D+ | 19 / 20 | `USB_D-`, `USB_D+` | — |
| UART0 TX / RX (CH340K) | 43 / 44 | `TXD/IO43`, `RXD/IO44` | — |
| BOOT button | 0 | `IO0` | Strapping |

### GPIO conflict matrix
| GPIO | Allocation | Free for external audio? |
|---|---|---|
| 0, 3, 45, 46 | Strapping (IO45/IO46 pull-downs on schematic; IO0 BOOT) | No |
| 1, 9–15, 21, 48 | LCD / touch | No |
| 2, 41, 42 | TF card | No (unless SD is abandoned — needs review) |
| 19, 20, 43, 44 | USB / UART console | No |
| 35, 36, 37 | Octal PSRAM inside the module if it is the 8 MB PSRAM (R8) variant; schematic leaves them unconnected with a "Check IO33~IO37" note | No |
| 8, 16, 17, 18, 38, 39, 40, 47 | Dangling labels only (`CSI_*`, `IO38/SCL`, `IO39/SDA`, `IO40`, `IO47/SPI_NCS2`); no connector or pad | Not accessible without module rework — do not use |
| **4, 5** (J1), **6, 7** (J2) | Expansion headers | **Only candidates** |

Corrections to the earlier uncommitted draft: LCD CS is **GPIO15**, not GPIO5; the TF card is 1-bit, so GPIO12/13/15 are **not** shared with SD data lines; GPIO5 is a free J1 signal, not LCD CS.

### H01 risks escalated to H03 (blocking)
- **Only four accessible GPIOs.** A mic plus MAX98357A needs BCLK, WS, mic DATA and amp DIN = 4 GPIOs with shared clocks, leaving **no GPIO for amplifier enable/SD_MODE**. Amp enable must then be hard-wired (startup-pop and mute behavior must be bench-checked), or another interface must be sacrificed with hardware-agent review. Do not assign audio pins until H03.
- **Shared I2S clocks for RX and TX** on one ESP32-S3 I2S port (full-duplex) must be validated with the chosen ESP-IDF driver.
- **Power:** headers provide only +3V3 from an LD1117; 5 V is not broken out. MAX98357A current at 3.3 V and LD1117 thermal headroom are unmeasured.
- **PCB revision:** physical revision marking not observed.

## Hardware discovery task
1. Download/inspect exact revision schematic under upstream `hardware/`, board photos and example `LVGL_Touch`.
2. Record SPI SCLK/MOSI/CS/DC/RST/backlight; NS2009 I2C SDA/SCL/address/interrupt if present; SD CS/bus sharing; all header GPIOs; USB/UART/strapping pins; power rails.
3. Verify physical header access and whether I2S needs extra breakout/soldering. Avoid conflicts with SPI/I2C/SD/USB/bootstrapping.
4. Select microphone (I2S MEMS e.g. INMP441-compatible, voltage verified) and amplifier (MAX98357A), identify BCLK, WS, DOUT (mic), DIN (amp), power, GND, amp shutdown/gain pins. Sharing BCLK/WS may be possible but requires validated driver config.
5. Confirm microphone audio level, 5V amplifier current supply, 3.3V logic compatibility, and speaker impedance/power. Use conservative gain and volume limits.
6. Create `config/board_pins.json` with evidence for each pin, plus a photo or schematic citation. No unverified pins in production.

## Bench bring-up sequence
- Flash display/touch demo; check rotation and 5-point calibration.
- Test amp with 1 kHz quiet sine then short prerecorded clip; measure current and distortion.
- Capture mic waveform, inspect peak/noise floor and sample clock, test speech capture with speaker silent.
- Test capture while playing effects; resolve self-recognition.
- Run ESP-SR model without games, then integrate.

## Electrical safety
Use enclosure, strain relief, inaccessible wiring, no exposed cells, no sharp edges, and limit acoustic output. Prototype USB-powered; don't assume battery circuitry exists.
