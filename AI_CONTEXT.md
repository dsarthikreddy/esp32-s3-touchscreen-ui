# AI_CONTEXT.md — Portable AI Coding Assistant Briefing

> **Use this file first** when starting a session in **ChatGPT, Claude, Gemini, Copilot, Cursor, or any other AI assistant**. It contains the complete, self-contained technical truth of this repository so you do not need prior chat history.

---

## 1. Project Identity
- **Firmware Name:** `cuteOS.ino` (internally branded `cuteOS v2.0`)
- **Target MCU:** ESP32-S3-N16R8 (Dual-core Xtensa LX7 @ 240 MHz, 16 MB QIO Flash, **8 MB OPI PSRAM**)
- **Display:** 3.5" 320x480 TFT LCD (`ILI9488`), rotated to landscape (`setRotation(1)` -> `480x320`)
- **Touch Controller:** `XPT2046` resistive touch sharing `SPI2_HOST` with the display
- **Graphics Stack:** `LovyanGFX` (`1.3.0`) using full-frame 16-bit RGB565 `LGFX_Sprite` buffers in OPI PSRAM (no LVGL currently)
- **Framework / Core:** Arduino framework on `esp32:esp32` core `2.0.17`

---

## 2. Repository Files
- `cuteOS.ino` — Firmware entry point, pin configuration, navigation, and shared rendering
- `PhoneLink.h`, `PhonePanels.h`, `MarketPanels.h`, `AtlasPanels.h` — BLE transport and feature panels
- `companion/` — Android app for the BLE link and market-data requests
- `secrets.example.h` — Sanitized template for Wi-Fi SSID/password, home lat/lon, and region labels
- `secrets.h` — **Gitignored** local configuration (must exist locally for user's Wi-Fi/location; falls back to `secrets.example.h` via `__has_include` if missing)
- `.gitignore` — Excludes `secrets.h`, `.env`, keys, biometric/private files, and build outputs
- Documentation files (`README.md`, `HANDOFF.md`, `GEMINI.md`, `HARDWARE.md`, `PIN_MAPPING.md`, `DISPLAY_TOUCH.md`, `UI_DESIGN.md`, `ARCHITECTURE.md`, `DEPENDENCIES.md`, `DECISIONS.md`, `BUGS.md`, `TEST_RESULTS.md`, `TASKS.md`, `CHANGELOG.md`)

---

## 3. Non-Negotiable Constraints (Read Before Editing Code)
1. **Do NOT change pin numbers** in `class LGFX` unless the user explicitly changed physical wiring.
2. **Keep `OPI PSRAM` enabled** (`PSRAM=opi`). The firmware allocates ~1.86 MB of sprites via `setPsram(true)` and halts with `"ENABLE OPI PSRAM!"` if any allocation fails.
3. **Preserve SPI bus sharing:** `cfg.bus_shared = true` and `cfg.use_lock = true` on `SPI2_HOST` (`SCLK=40`, `MOSI=39`, `MISO=38`) are required because `ILI9488` (`CS=1`) and `XPT2046` (`CS=42`) share the same SPI bus.
4. **Never hardcode secrets** in `cuteOS.ino` or any tracked file. Put Wi-Fi credentials, GPS coordinates, API keys, and personal labels only in `secrets.h`.
5. **Maintain ESP32 Arduino Core 2.0.17 compatibility** (and keep the `ESP_ARDUINO_VERSION_MAJOR >= 3` guard on `rgbLedWrite` / `neopixelWrite`).
6. **Do not rewrite UI to LVGL** unless explicitly asked to perform a major migration (see `ARCHITECTURE.md` Section 7 for LVGL assessment).

---

## 4. Verified Pinout Summary (`cuteOS.ino` lines 43–122)
- **Shared SPI2 Bus (`SPI2_HOST`):** `SCLK=40`, `MOSI=39`, `MISO=38`
- **ILI9488 Display:** `DC=41`, `CS=1`, `RST=3`, `BUSY=-1` (`freq_write=20MHz`, `freq_read=16MHz`, `readable=false`)
- **XPT2046 Touch:** `CS=42`, `INT=-1` (`freq=1.5MHz`)
- **Backlight PWM (Optional):** `GPIO 14` (`BACKLIGHT_PWM_PIN = 14`, software RGB565 dimming also runs on `canvas`)
- **ADC Oscilloscope Input:** `GPIO 4` (`ADC_SCOPE_PIN = 4`, 12-bit `0..4095`)
- **Digital Output GPIOs:** `GPIO 5`, `GPIO 6`, `GPIO 7` (`OUT_GPIOS[3]`)
- **Onboard WS2812 RGB LED:** `GPIO 48` (`ONBOARD_RGB_PIN`)

---

## 5. Architecture & Key Symbols
- **PSRAM Sprites:**
  - `bgSprite` (480x320): Everest photo or procedural alpine fallback
  - `frostSprite` (480x320): Pre-baked white glass blur over `bgSprite`
  - `darkGlass` (480x320): Pre-baked dark glass blur over `bgSprite`
  - `mapSprite` (480x320): Zoom-17 OSM/Esri map or offline street map
  - `canvas` (480x320): Main frame composition buffer pushed to `tft`
  - `newsImg[5]` (240x135 each): PSRAM sprites for BBC RSS story images
- **Screen IDs (`currentScreen`):**
  `SCR_HOME(0)`, `SCR_NEWS(1)`, `SCR_ARTICLE(2)`, `SCR_CONTROL(3)`, `SCR_APPS(4)`, `SCR_TILES(5)`, `SCR_WIDGETS(6)`, `APP_MAP(7)`, `APP_WEATHER(8)`, `APP_RADAR(9)`, `APP_SCOPE(10)`, `APP_RGB(11)`, `APP_CALC(12)`, `APP_TIMER(13)`.
- **Modular Widgets (`homeSlot[2]`):**
  `W_WEATHER(0)`, `W_MINIMAP(1)`, `W_NEWS(2)`, `W_RADAR(3)`, `W_GPIO(4)`, `W_TIMER(5)`, `W_CORE(6)`.
- **NVS Persistence (`Preferences`, namespace `"cuteos"`):**
  - `"bri"` (`int`): Screen brightness `20..100`
  - `"w0"`, `"w1"` (`int`): Top/bottom home widget indices
  - `"cal"` (`uint16_t[8]`): `LovyanGFX` touch calibration matrix

---

## 6. Build & Verification Commands (Windows PowerShell)
```powershell
$cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
& $cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio" .
```
Check before finishing any task:
- Compile exits `0` with no errors.
- `git diff` / `git status` shows no secrets (`WIFI_PASS`, coordinates, tokens) in tracked files.
- State clearly what was verified by compilation/static checks vs. what requires physical ESP32-S3 hardware testing.
