# CHANGELOG.md

All notable changes to this project are documented in this file.

---

## [2.0.1] — 2026-11-27 (Documentation, Security & Repository Release)
### Added
- `.gitignore` excluding local secrets (`secrets.h`, `.env`, keys, biometric/private files) and Arduino/PlatformIO build outputs.
- `secrets.example.h` sanitized configuration template for Wi-Fi credentials, GPS coordinates, and location labels.
- Complete 15-document engineering & beginner onboarding suite (`README.md`, `AI_CONTEXT.md`, `HANDOFF.md`, `GEMINI.md`, `HARDWARE.md`, `PIN_MAPPING.md`, `DISPLAY_TOUCH.md`, `UI_DESIGN.md`, `ARCHITECTURE.md`, `DEPENDENCIES.md`, `DECISIONS.md`, `BUGS.md`, `TEST_RESULTS.md`, `TASKS.md`, `CHANGELOG.md`).
- Private GitHub repository (`esp32-s3-touchscreen-ui`) initialized and verified.

### Changed
- Extracted hardcoded Wi-Fi credentials and private home coordinates/labels from [cuteOS.ino](cuteOS.ino) into gitignored `secrets.h` using `#if __has_include("secrets.h")` with `secrets.example.h` fallback. Preserved 100% of existing firmware behavior and local configuration.

---

## [2.0.0] — Baseline Firmware (`cuteOS v2.0`)
### Included Features
- Standalone ESP32-S3-N16R8 + 3.5" ILI9488 + XPT2046 touchscreen OS using `LovyanGFX` `1.3.0` and OPI PSRAM sprites.
- 14 screens, 7 modular home widgets, Dynamic Island hub, 5-tile BBC RSS live news with thumbnails, Zoom-17 OSM/Esri local map, Open-Meteo weather & 3-day forecast, 2.4 GHz RF radar, `GPIO 4` ADC oscilloscope, onboard WS2812 RGB & `GPIO 5/6/7` studio, calculator, focus timer, and NVS-persisted touch calibration + brightness.
