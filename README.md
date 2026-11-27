# cuteOS v2.0 — ESP32-S3 Touchscreen Smart OS

A standalone, glassy smart display firmware for the **ESP32-S3-N16R8** paired with a **3.5-inch 320x480 ILI9488 SPI TFT** and **XPT2046 resistive touchscreen**, built with **LovyanGFX**.

> **Project Status:** Firmware compiles cleanly for `esp32:esp32:esp32s3` (16 MB Flash, OPI PSRAM). Runtime hardware verification on physical hardware was not performed in this documentation/repository setup session—see [TEST_RESULTS.md](TEST_RESULTS.md) for what is verified vs. untested.

---

## Features

- **Custom Boot Splash:** Vector-drawn circular twin-peak mountain emblem with sun, clouds, and progress bar (`drawBootSplash`).
- **Pre-Baked Liquid Glass UI:** Uses an 8 MB OPI PSRAM wallpaper buffer (`bgSprite`) and two pre-blended glass buffers (`frostSprite`, `darkGlass`) rendered at 480x320 RGB565.
- **Dynamic Island Hub:** Expandable top pill displaying Wi-Fi status, live temperature or active focus timer countdown, and quick-launch pills (`News`, `Map`, `Bright`, `Close`).
- **Home Screen & Modular Widgets:** Live NTP clock (`HH:MM`), location card, 5-icon squircle bottom dock, and 2 customizable widget slots (`Weather`, `Area Map`, `Live News`, `RF Radar`, `GPIO 5-7`, `Focus Timer`, `ESP32 Core`) persisted in NVS flash.
- **5-Tile Live News Reader:** Fetches BBC News RSS feeds across 5 topics (`Top`, `India`, `Tech`, `Science`, `Sport`), downloads story thumbnails into PSRAM sprites, and opens a full article summary view.
- **Real Local Map (Street & Satellite):** Fetches 3x3 slippy map tiles at Zoom 17 from OpenStreetMap (PNG) or Esri World Imagery (JPEG), with an offline vector street fallback.
- **Weather & 3-Day Forecast:** Queries Open-Meteo API for current temperature, humidity, wind speed, WMO weather condition, and 3-day high/low forecast.
- **Control Center:** Interactive screen brightness slider (software RGB565 scaling + optional GPIO 14 PWM), Wi-Fi reconnect, Everest wallpaper download, map shortcut, RGB lamp cycle, RF scan, touch calibration, GPIO reset, and reboot.
- **Built-in Tools & Apps:**
  - **2.4 GHz Wi-Fi RF Radar:** Scans channels 1–13 and displays signal bars + top 4 SSIDs/RSSI.
  - **ADC Oscilloscope:** Live 12-bit waveform plotter on `GPIO 4` (`0–3.3 V`) with hold/resume and core temperature readout.
  - **RGB & GPIO Studio:** Controls the onboard WS2812 RGB LED (`GPIO 48`) with 7 presets + breathing mode, plus 3 digital outputs (`GPIO 5`, `GPIO 6`, `GPIO 7`).
  - **Calculator:** 4-function floating-point calculator (`+`, `-`, `*`, `/`).
  - **Focus Timer:** Circular ring gauge timer with `5 min`, `15 min`, and `25 min Pomodoro` presets.
- **Persistent Touch Calibration:** 3-sample median filtering + EMA smoothing; calibration matrix stored permanently in ESP32 NVS (`Preferences`).

---

## Quick Start (Beginner Friendly)

### 1. Prerequisites
- **Hardware:** ESP32-S3-N16R8 (16 MB Flash, 8 MB Octal/OPI PSRAM) + 3.5" 320x480 ILI9488 SPI TFT with XPT2046 touch (14-pin red PCB module). See [HARDWARE.md](HARDWARE.md) and [PIN_MAPPING.md](PIN_MAPPING.md).
- **Software:** [Arduino IDE 2.x](https://www.arduino.cc/en/software) (tested with bundled `arduino-cli` v1.5.1).

### 2. Install Board Core & Libraries
1. Open Arduino IDE -> **File -> Preferences -> Additional Boards Manager URLs**, add:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. Open **Tools -> Board -> Boards Manager**, search `esp32` by **Espressif Systems**, and install version **`2.0.17`** (verified version; see [DEPENDENCIES.md](DEPENDENCIES.md) regarding Core 3.x notes).
3. Open **Tools -> Manage Libraries**, search `LovyanGFX` by **lovyan03**, and install version **`1.3.0`**.

### 3. Configure Wi-Fi & Location Secrets
1. Name the project folder `cuteOS` so Arduino can find `cuteOS.ino`. In that folder, copy `secrets.example.h` to `secrets.h`:
   ```powershell
   Copy-Item secrets.example.h secrets.h
   ```
2. Open `secrets.h` and enter your own 2.4 GHz Wi-Fi credentials (`SECRET_WIFI_SSID`, `SECRET_WIFI_PASS`), latitude/longitude (`SECRET_HOME_LAT`, `SECRET_HOME_LON`), area/city labels, and UTC offset (`SECRET_NTP_OFFSET_SEC`).
3. **Never commit `secrets.h`**—it is excluded by `.gitignore`.

### 4. Select Exact Arduino IDE Board Settings
Set these exact options under **Tools** before compiling or uploading:

| Arduino IDE Setting | Required Value | Notes |
|---|---|---|
| **Board** | `ESP32S3 Dev Module` | `esp32:esp32:esp32s3` |
| **PSRAM** | **`OPI PSRAM`** | **Mandatory** (`cuteOS` halts without 8 MB OPI PSRAM) |
| **Flash Size** | `16MB (128Mb)` | Matches N16R8 module |
| **Partition Scheme** | `Huge APP (3MB No OTA/1MB SPIFFS)` | Or `16M Flash (3MB APP/9.9MB FATFS)` |
| **Flash Mode** | `QIO 80MHz` | Standard for N16R8 |
| **CPU Frequency** | `240MHz (WiFi)` | Required for smooth UI & Wi-Fi |
| **USB Mode** | `Hardware CDC and JTAG` | Or `USB-OTG` depending on board USB port |
| **Upload Speed** | `921600` | Drop to `115200` if flashing fails |

### 5. Compile & Upload
- **Arduino IDE:** Open `cuteOS.ino`, select your COM port under **Tools -> Port**, and click **Upload**.
- **Command Line (`arduino-cli`):**
  ```powershell
  arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio" .
  arduino-cli upload -p COMx --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio" .
  ```
  *(Replace `COMx` with your ESP32-S3 serial port.)*

---

## Navigation & Gestures

| From Screen | Gesture / Action | Destination |
|---|---|---|
| **Home** | Swipe Left -> Right | 5-Tile Live News Feed |
| **Home** | Swipe Right -> Left | 4 Tech Tiles & Notifications |
| **Home** | Swipe Down | Control Center (Brightness & Quick Actions) |
| **Home** | Swipe Up | App Drawer (8 Squircle Icons) |
| **Home** | Tap Dynamic Island | Expand Quick Hub (`News`, `Map`, `Bright`, `Close`) |
| **Home** | Tap `+ Widgets` | Customize Top & Bottom Home Widgets |
| **Sub-screens** | Swipe Down (or tap `< Home` / `< Apps`) | Return to Home or App Drawer |
| **Article Reader** | Swipe Left / Right | Next / Previous Story |

---

## Documentation Index

| File | Purpose |
|---|---|
| [AI_CONTEXT.md](AI_CONTEXT.md) | Model-agnostic context for ChatGPT, Claude, Gemini, etc. |
| [HANDOFF.md](HANDOFF.md) | Engineering handoff, code map, and next steps |
| [GEMINI.md](GEMINI.md) | Gemini / Antigravity workspace guide |
| [HARDWARE.md](HARDWARE.md) | MCU, display, touch, and peripheral hardware details |
| [PIN_MAPPING.md](PIN_MAPPING.md) | Verified GPIO pinout and wiring table |
| [DISPLAY_TOUCH.md](DISPLAY_TOUCH.md) | LovyanGFX SPI, ILI9488, XPT2046, calibration, and brightness |
| [UI_DESIGN.md](UI_DESIGN.md) | Screen inventory, palette, typography, widgets, and gestures |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Software architecture, memory budget, and LVGL feasibility |
| [DEPENDENCIES.md](DEPENDENCIES.md) | Core, library versions, APIs, and installation steps |
| [DECISIONS.md](DECISIONS.md) | Architecture Decision Records (ADRs) |
| [BUGS.md](BUGS.md) | Known issues, edge cases, and technical debt |
| [TEST_RESULTS.md](TEST_RESULTS.md) | Compilation verification and tested vs. untested matrix |
| [TASKS.md](TASKS.md) | Feature status and future roadmap |
| [CHANGELOG.md](CHANGELOG.md) | Release and maintenance history |
