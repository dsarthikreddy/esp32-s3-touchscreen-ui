# TEST_RESULTS.md — Build Verification & Hardware Test Matrix

This document records firmware build checks and owner-reported testing on physical hardware.

---

## 1. Software & Compilation Tests (Executed & Verified)

**Environment:**
- `arduino-cli` `1.5.1`
- `esp32:esp32` core `2.0.17` (`xtensa-esp32s3-elf-gcc` `esp-2021r2-patch5-8.4.0`)
- `LovyanGFX` `1.3.0`
- Target FQBN: `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio`

| Test | Scope | Result | Measured Output |
|---|---|---|---|
| **Baseline Firmware Compile** | Original `cuteOS.ino` before sanitization | **PASS** (exit `0`) | Flash: `1,071,321` bytes (`34%` of `3,145,728`); Global SRAM: `50,756` bytes (`15%` of `327,680`, `276,924` free) |
| **Sanitized Local Compile** | `cuteOS.ino` + gitignored `secrets.h` | **PASS** (exit `0`) | Flash: `1,072,365` bytes (`34%` of `3,145,728`); Global SRAM: `50,756` bytes (`15%` of `327,680`, `276,924` free) |
| **Clean-Clone Compile** | Isolated temp folder with `cuteOS.ino` + `secrets.example.h` (no `secrets.h`) | **PASS** (exit `0`) | Flash: `1,072,349` bytes (`34%` of `3,145,728`); Global SRAM: `50,756` bytes (`15%` of `327,680`, `276,924` free) |
| **Static Secret & PII Scan** | All files excluding `secrets.h` | **PASS** (`0` matches) | Zero Wi-Fi passwords, SSIDs, coordinates, or personal location strings in tracked files |
| **Connected Board Detection** | `arduino-cli board list` | **No boards found** | Confirmed no ESP32-S3 attached via USB during test session |

---

## 2. Physical Hardware & Runtime Matrix

The owner reported testing the subsystems below on the ESP32-S3-N16R8 and observing the firmware output. The last column lists checks for reproducing the tests.

| Subsystem | Verification Status | On-device checks |
|---|---|---|
| **OPI PSRAM Initialization** | Tested on hardware (owner-reported) | Boot splash appears; `"ENABLE OPI PSRAM!"` halt screen does not trigger |
| **ILI9488 SPI Display (`CS=1, DC=41, RST=3`)** | Tested on hardware (owner-reported) | Clean image, correct RGB colors (`spriteIsSwapped` detection), no SPI corruption at 20 MHz |
| **XPT2046 Touch & Calibration (`CS=42`)** | Tested on hardware (owner-reported) | 4-corner calibration completes, saves to NVS `"cuteos"/"cal"`, taps & swipes respond accurately |
| **Backlight Brightness (`GPIO 14` vs `3.3V`)** | Tested on hardware (owner-reported) | Slider dims smoothly via software scaling and/or GPIO 14 PWM |
| **Wi-Fi, NTP & HTTPS Endpoints** | Tested on hardware (owner-reported) | Connects to 2.4 GHz Wi-Fi, syncs IST/local time, loads Everest JPEG, OSM/Esri map tiles, Open-Meteo weather, BBC RSS + images |
| **RF Spectrum Radar** | Tested on hardware (owner-reported) | Channel 1–13 scan populates bars and top 4 SSIDs |
| **ADC Oscilloscope (`GPIO 4`)** | Tested on hardware (owner-reported) | Reads `0.00–3.30 V` accurately with external signal; verify voltage divider for >3.3 V signals |
| **GPIO Outputs (`5, 6, 7`) & RGB (`GPIO 48`)** | Tested on hardware (owner-reported) | Pins toggle 0V/3.3V cleanly; onboard WS2812 changes color and breathes |
| **NVS Persistence (`bri`, `w0`, `w1`, `cal`)** | Tested on hardware (owner-reported) | Settings survive power cycle / `ESP.restart()` |

---

## 3. How to Re-Run Compilation Verification Locally

```powershell
$cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
& $cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio" .
```
