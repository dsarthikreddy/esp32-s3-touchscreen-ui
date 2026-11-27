# DEPENDENCIES.md — Toolchain, Libraries & External Services

All installed versions below were verified directly against the local development environment.

---

## 1. Compiler Toolchain & Board Core

| Component | Verified Installed Version | Notes |
|---|---|---|
| **Arduino CLI** | `1.5.1` (bundled with Arduino IDE) | Located under `%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe` |
| **ESP32 Arduino Core** | `esp32:esp32` **`2.0.17`** | Latest available is `3.3.12`; `2.0.17` is verified working |
| **Compiler (`xtensa-esp32s3-elf-gcc`)** | `esp-2021r2-patch5-8.4.0` | Installed automatically by `esp32:esp32` core |
| **Uploader (`esptool_py`)** | `4.5.1` | Installed automatically by `esp32:esp32` core |

> **ESP32 Core 2.x vs 3.x Note:** [AURAOS.ino](AURAOS.ino) includes a compile-time guard (`ESP_ARDUINO_VERSION_MAJOR >= 3`) for `rgbLedWrite` vs `neopixelWrite`. However, compilation and runtime behavior have only been verified on **`2.0.17`**. If upgrading to `3.x`, test `analogWrite`, `WiFiClientSecure`, and `LovyanGFX` `1.3.0` carefully.

---

## 2. Third-Party & Core Libraries

| Library | Version | Source | Purpose in `AURAOS.ino` |
|---|---|---|---|
| **LovyanGFX** (`lovyan03`) | **`1.3.0`** | Arduino Library Manager (`user` libs) | ILI9488 SPI display, XPT2046 touch, PSRAM sprites, JPEG/PNG decoding, FreeSerif fonts |
| **WiFi** | Built into `esp32:esp32` `2.0.17` | Espressif Core | Station mode (`WIFI_STA`), 2.4 GHz RF channel scan |
| **WiFiClientSecure** | Built into `esp32:esp32` `2.0.17` | Espressif Core | TLS transport for HTTPS feeds (`setInsecure()`) |
| **HTTPClient** | Built into `esp32:esp32` `2.0.17` | Espressif Core | HTTP GET with redirect following for images, tiles, RSS, JSON |
| **Preferences** | Built into `esp32:esp32` `2.0.17` | Espressif Core | NVS flash storage (`"cuteos"` namespace) |
| **time.h / math.h** | Built into ESP-IDF / Newlib | Standard C / POSIX | `configTime()`, `getLocalTime()`, trig/rounding math |

*(Other libraries present on the owner's system—such as `Adafruit_GFX`, `MFRC522`, `NimBLE-Arduino`, `ESP32Servo`—are **not** used by `AURAOS.ino`.)*

---

## 3. External Network Services & Endpoints (No API Keys Required)

| Feature | Provider / Endpoint | Protocol / Format | Failure Fallback |
|---|---|---|---|
| **Time Sync** | `pool.ntp.org`, `time.google.com` | NTP (UDP 123) | Uptime-based placeholder clock (`12:00+`) |
| **Wallpaper** | `commons.wikimedia.org` (`Special:FilePath/...?width=480`) | HTTPS -> JPEG (`<= 160 KB`) | Procedural alpine mountain (`buildPhotorealisticEverestFallback`) |
| **News Feeds (5 topics)** | `feeds.bbci.co.uk` (Top, India, Tech, Science, Sport) | HTTPS -> RSS XML (`<= 48 KB`) | `"Tap Refresh for Live News"` placeholders |
| **News Thumbnails** | BBC `ichef` CDN URLs parsed from RSS `<media:thumbnail>` | HTTPS -> JPEG/PNG (`<= 70 KB`) | Gradient topic placeholder (`drawNewsPlaceholder`) |
| **Street Map** | `tile.openstreetmap.org/{z}/{x}/{y}.png` (Zoom 17) | HTTPS -> PNG (`<= 120 KB`/tile) | Vector street fallback (`buildOfflineAreaMap`) |
| **Satellite Map** | `server.arcgisonline.com/.../World_Imagery/MapServer/tile/{z}/{y}/{x}` | HTTPS -> JPEG (`<= 120 KB`/tile) | Vector street fallback (`buildOfflineAreaMap`) |
| **Weather & Forecast** | `api.open-meteo.com/v1/forecast` | HTTPS -> JSON | Static defaults (`26 C`, `Partly Cloudy`, 3-day defaults) |

---

## 4. Exact Installation Commands (`arduino-cli`)

```powershell
$cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
& $cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
& $cli core install esp32:esp32@2.0.17 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
& $cli lib install "LovyanGFX@1.3.0"
```
