# TASKS.md — Feature Status & Future Roadmap

---

## 1. Completed Features (Implemented in `AURAOS.ino`)

- [x] **Hardware Driver Setup:** `LovyanGFX` `1.3.0` (`ILI9488` + `XPT2046` on shared `SPI2_HOST`)
- [x] **OPI PSRAM Compositor:** 5 full-screen 480x320 sprites + 5 news image sprites (`~1.82 MB`)
- [x] **Boot Splash:** Twin-peak mountain emblem, golden sun, clouds, progress bar
- [x] **Liquid Glass UI:** Pre-baked `frostSprite` & `darkGlass` with rounded corner restoration
- [x] **Home Screen & Dynamic Island:** Clock, area card, 5-icon squircle dock, expandable hub
- [x] **Modular Widgets:** 7 widget types assignable to 2 home slots, persisted in NVS
- [x] **Live News Reader:** 5 BBC RSS topics, 5 stories per topic, real JPEG/PNG thumbnails, article summary view
- [x] **Local Area Map:** Zoom-17 OSM Street + Esri Satellite 3x3 tile stitcher + offline fallback
- [x] **Weather & Forecast:** Open-Meteo current conditions + 3-day forecast + WMO vector icons
- [x] **Control Center:** Brightness slider (`20–100%`), Wi-Fi, Everest wallpaper, Map, RGB, RF Scan, Touch Calib, GPIO Reset, Reboot
- [x] **Tech Tiles & Notifications:** 4 swipeable telemetry/notification pages
- [x] **Built-in Apps:** 2.4 GHz RF Radar, `GPIO 4` ADC Oscilloscope, RGB (`GPIO 48`) & GPIO (`5,6,7`) Studio, Calculator, Focus Timer
- [x] **Touch Engine:** 3-sample median filter, EMA smoothing, swipe gestures, NVS touch calibration
- [x] **Repository & Secret Safety:** `.gitignore`, `secrets.example.h`, `secrets.h` extraction, full documentation suite

---

## 2. Incomplete / Partial Features

- [ ] **Hardware Backlight Dimming Wiring:** Firmware drives `GPIO 14`, but physical hardware may still have `LED` tied to `3.3V` (software dimming compensates).
- [ ] **Map Pan & Zoom Controls:** `APP_MAP` currently renders a fixed Zoom-17 view centered on `HOME_LAT`/`HOME_LON` without touch drag panning or zoom `+`/`-`.
- [ ] **Wi-Fi Provisioning UI:** Credentials must be configured at compile time in `secrets.h` (no SoftAP captive portal or on-screen keyboard yet).
- [ ] **Chained Calculator Arithmetic:** Pressing multiple operators consecutively (`2 + 3 *`) without `=` does not auto-evaluate intermediate results.
- [ ] **Article Body Scraping:** Displays RSS `<description>` summary (usually 1–3 sentences) rather than full web article text.

---

## 3. Future Roadmap

### Short-Term Polish
- [ ] Add `arduino-cli` compile check to CI (GitHub Actions workflow) using `secrets.example.h` fallback.
- [ ] Auto-trigger News + Map + Everest fetch after manual Control Center Wi-Fi reconnect if not yet loaded.
- [ ] Sort `topSSIDs` by RSSI descending in `runRFSpectrumScan()`.
- [ ] Display `"ERR"` on calculator divide-by-zero instead of `0`.

### Medium-Term Architecture Upgrades
- [ ] Move `downloadBufferHTTPS()` and `fetchLocalWeather()` to a FreeRTOS worker task on **Core 0** so the UI stays responsive during downloads.
- [ ] Replace `WiFiClientSecure::setInsecure()` with Espressif `esp_crt_bundle` for verified TLS certificates.
- [ ] Cache downloaded Everest wallpaper / map tiles to LittleFS or an SD card on the 14-pin PCB for instant offline boot.
- [ ] Add SoftAP / BLE Wi-Fi provisioning + on-screen keyboard.

### Long-Term UI Migration
- [ ] Evaluate and prototype `LVGL v8.3` / `v9` on a feature branch (see [ARCHITECTURE.md](ARCHITECTURE.md) §6).
