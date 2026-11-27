# HANDOFF.md — Engineering Handoff Guide

## 1. Current State Summary
- **Working Codebase:** [AURAOS.ino](AURAOS.ino) (`cuteOS v2.0`) is a complete standalone ESP32-S3 touchscreen OS written on top of `LovyanGFX` (`1.3.0`) and `esp32:esp32` (`2.0.17`).
- **Sanitization Applied:** Hardcoded Wi-Fi credentials and exact home coordinates/estate strings were extracted into a gitignored `secrets.h` (which remains on the local machine so the owner's build behavior is 100% unchanged), with [secrets.example.h](secrets.example.h) committed as a safe template.
- **Compilation Status:** Verified compiling cleanly via `arduino-cli` (`esp32:esp32:esp32s3` with `FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi`).
- **Hardware Runtime Status:** **Not tested on physical hardware during this handoff** (no ESP32-S3 board was connected over USB). Treat runtime touch calibration, Wi-Fi/HTTPS downloads, and GPIO electrical levels as unverified until flashed by the owner.

---

## 2. Code Map of `AURAOS.ino`

| Section | Approx. Lines | Responsibility |
|---|---|---|
| Header & Includes | 1–28 | Includes `LovyanGFX.hpp`, `WiFi`, `WiFiClientSecure`, `HTTPClient`, `Preferences`, `secrets.h` / `secrets.example.h` |
| 1. Wi-Fi, Location & News Config | 29–46 | `WIFI_SSID`, `WIFI_PASS`, `HOME_LAT`, `HOME_LON`, `AREA_NAME`, `CITY_NAME`, 5 BBC RSS `TOPIC_URLS` |
| 2. Hardware Pinout (`LGFX`) | 48–137 | `SPI2_HOST`, `Panel_ILI9488`, `Touch_XPT2046`, PSRAM sprite globals, `BACKLIGHT_PWM_PIN`, `ADC_SCOPE_PIN`, `OUT_GPIOS`, `ONBOARD_RGB_PIN` |
| 3. Color Palette & Constants | 139–200 | RGB565 helpers (`makeRgb`, `blend565`), palette constants, `SCR_*`/`APP_*` IDs, `W_*` widget IDs |
| 4. Global State | 202–293 | UI state, brightness, Wi-Fi/NTP flags, `NewsArticle[5]`, weather, RF radar, oscilloscope buffer, RGB, calculator, timer, notifications, touch filter state |
| 5. Brightness & Drawing Helpers | 295–454 | `applyHardwarePWM`, `applySoftwareBrightnessRect`, `pushFullFrame`, `pushPartial`, `blitScaledNewsImg`, `drawWrappedText` |
| 6. Boot Splash | 456–525 | `drawBootSplash`, `updateBootProgress` (direct `tft` draws before loop) |
| 7. HTTPS & Glass Buffers | 527–642 | `rebuildGlassBuffers`, `buildPhotorealisticEverestFallback`, `downloadBufferHTTPS`, `downloadRealEverestPhoto` |
| 8. Live News Engine | 644–777 | XML/CDATA tag stripper, `<media:thumbnail>` URL extractor, `fetchLiveNews` |
| 9. Local Area Map | 779–834 | `buildOfflineAreaMap`, `downloadAreaRealMap` (Zoom 17 slippy tiles from OSM / Esri) |
| 10. Glass Cards & Squircle Icons | 836–1017 | `restoreRoundedCorners`, `drawFrostGlassCard`, `drawAppleSquircle`, 10 vector app/weather/ring icons |
| 11. Dynamic Island & Widgets | 1019–1172 | `drawDynamicIsland`, `drawSubHeader`, `drawModularWidget` |
| 12. Screen Renderers | 1174–1648 | All 14 screen rendering functions + `drawToastOverlay` |
| 13. Weather, NTP, RF & Render | 1650–1787 | `fetchLocalWeather` (Open-Meteo), `updateTimeFromNtp`, `runRFSpectrumScan`, `renderCurrentScreen`, `runAndSaveTouchCalibration` |
| 14. Touch & Gesture Engine | 1789–2092 | `getFilteredTouch`, `handleCalcKey`, `handleHomeWidgetTap`, `handleTap`, `handleSwipe`, `pollTouch` |
| 15. `setup()` & `loop()` | 2094–2242 | NVS load, OPI PSRAM sprite allocation, calibration check, Wi-Fi boot sync, main polling loop |

---

## 3. Critical Gotchas for the Next Engineer
1. **PSRAM Must Be `OPI PSRAM`:** If flashed with `PSRAM=disabled` or `PSRAM=enabled` (QSPI) on an N16R8 board, `createSprite` fails or crashes and displays `"ENABLE OPI PSRAM!"`.
2. **Core 2.0.17 vs Core 3.x:** Currently compiled on `esp32:esp32` **`2.0.17`**. Note `analogWrite` is used on `BACKLIGHT_PWM_PIN` (GPIO 14), and `#if ESP_ARDUINO_VERSION_MAJOR >= 3` guards `rgbLedWrite` vs `neopixelWrite`. Upgrading to Espressif Core 3.x requires re-testing ADC, PWM/LEDC, Wi-FiClientSecure, and `LovyanGFX` `1.3.0` compatibility.
3. **Blocking Network Calls:** `downloadBufferHTTPS` and `fetchLocalWeather` run synchronously on the main thread (up to 5–6.5 s timeout per request). During boot or manual refresh, touch input is paused until the HTTP call finishes.
4. **TLS Certificate Verification Disabled:** `WiFiClientSecure::setInsecure()` is used for Wikimedia, BBC, OSM, Esri, and Open-Meteo so root CAs don't need updating, at the expense of MITM vulnerability (acceptable for read-only public feeds, not for authenticated APIs).
5. **Backlight Wiring Detail:** Comment on line 123 states software dimming works when the display LED pin is wired to `3V3`, while hardware PWM works if moved to `GPIO 14`. Verify the user's physical LED pin connection before debugging brightness.

---

## 4. Immediate Next Steps for the Owner
1. Connect the ESP32-S3-N16R8 via USB and upload `AURAOS.ino` with **Tools -> PSRAM -> OPI PSRAM**.
2. Complete the 4-corner touch calibration on first boot (saved to NVS flash).
3. Verify boot Wi-Fi connection, Everest photo fetch, map tiles, weather, and news feeds on Serial Monitor (`115200` baud).
4. Review [BUGS.md](BUGS.md) and [TASKS.md](TASKS.md) before adding new features or attempting an LVGL migration.
