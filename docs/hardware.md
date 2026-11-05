# Hardware validation checklist — BLOCKER BEFORE WIRING
Source: https://github.com/Makerfabs/MaTouch_Lite_ESP32-S3-SPI-IPS-with-Touch-2.4-ST7789
Makerfabs README states ESP32-S3, 320×240 ST7789V SPI, NS2009 resistive touch, 16 MB flash, 8 MB PSRAM, microSD interface, 2× JST 1.25mm 4-pin expansion headers, USB-C power. **Actual GPIO pin mapping is not established in this pack.**

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
