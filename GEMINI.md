# GEMINI.md — Gemini & Antigravity Project Instructions

This file provides workspace context for Gemini CLI, Antigravity IDE, and Gemini-based coding assistants. For full cross-assistant details, also read [AI_CONTEXT.md](AI_CONTEXT.md).

## Project Summary
- **Repository:** `esp32-s3-touchscreen-ui` (`cuteOS.ino` / `cuteOS v2.0`)
- **MCU:** ESP32-S3-N16R8 (16 MB Flash, 8 MB OPI PSRAM)
- **Display & Touch:** 3.5" 320x480 `ILI9488` SPI TFT + `XPT2046` resistive touch via `LovyanGFX` `1.3.0`
- **Core:** Arduino `esp32:esp32` `2.0.17`

## Mandatory Workspace Rules
1. **Never modify or expose `secrets.h`:** Wi-Fi credentials (`SECRET_WIFI_SSID`, `SECRET_WIFI_PASS`), home GPS coordinates (`SECRET_HOME_LAT`, `SECRET_HOME_LON`), and personal location labels live exclusively in gitignored `secrets.h`. Use `secrets.example.h` for schema changes.
2. **Preserve hardware configuration:** `SPI2_HOST` (`SCLK=40`, `MOSI=39`, `MISO=38`), Display (`CS=1`, `DC=41`, `RST=3`), Touch (`CS=42`), Backlight (`14`), ADC (`4`), Outputs (`5,6,7`), RGB (`48`).
3. **Preserve OPI PSRAM requirement:** All offscreen sprites (`bgSprite`, `frostSprite`, `darkGlass`, `mapSprite`, `canvas`, `newsImg[5]`) rely on `setPsram(true)` and `PSRAM=opi`.
4. **Keep working behavior intact:** Do not refactor the LovyanGFX rendering pipeline into LVGL unless the user explicitly initiates an LVGL migration task.
5. **Verify builds before reporting success:** Compile using `arduino-cli` with:
   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio" .
   ```
6. **Be explicit about hardware testing:** Never claim on-device touch, Wi-Fi, display, or GPIO verification unless a physical ESP32-S3 board was actually flashed and observed.
