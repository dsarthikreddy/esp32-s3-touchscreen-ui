# HARDWARE.md — Hardware Architecture & Specifications

> **Verification Legend:**
> - **Verified (Code):** Directly defined and configured in [AURAOS.ino](AURAOS.ino).
> - **Assumed / Hardware-Dependent:** Expected physical hardware characteristics inferred from the firmware comments and board model, not physically inspected or probed on a bench during this session.

---

## 1. Microcontroller Module — ESP32-S3-N16R8

| Parameter | Specification | Verification |
|---|---|---|
| **Module / Variant** | ESP32-S3-N16R8 | Verified (code comments & `OPI PSRAM` check) |
| **CPU** | Dual-core Xtensa LX7 @ 240 MHz | Verified (code + `CPUFreq=240`) |
| **Flash Memory** | 16 MB (`N16`, Quad/QIO 80 MHz) | Assumed from `N16R8` part code; compiled with `FlashSize=16M` |
| **PSRAM** | 8 MB Octal SPI (`R8` / `OPI PSRAM`) | Verified required (`PSRAM=opi`, `createSprite` halts if missing) |
| **Wi-Fi** | 2.4 GHz 802.11 b/g/n (`WIFI_STA`) | Verified (`WiFi.h`, RF scan channels 1–13) |
| **Onboard RGB LED** | WS2812 / NeoPixel on `GPIO 48` | Verified in code (`#define ONBOARD_RGB_PIN 48`) |
| **Core Temp Sensor** | Internal ESP32-S3 temperature sensor (`temperatureRead()`) | Verified in code (`drawAppScope`, `drawTilesScreen`) |
| **DevKit Form Factor** | Dual-USB ESP32-S3 DevKitC-1 style N16R8 board | Assumed (typical board carrying GPIO 48 WS2812 + N16R8) |

---

## 2. Display & Touch Module — 3.5" ILI9488 + XPT2046

| Parameter | Specification | Verification |
|---|---|---|
| **Form Factor** | 14-pin Red PCB 3.5" SPI TFT Touch Module | Verified from firmware comment (line 47) |
| **Display Driver IC** | `ILI9488` | Verified (`lgfx::Panel_ILI9488`) |
| **Native Panel Resolution** | `320 x 480` portrait | Verified (`panel_width = 320`, `panel_height = 480`) |
| **Active OS Resolution** | `480 x 320` landscape (`setRotation(1)`) | Verified (`tft.setRotation(1)` + 480x320 sprites) |
| **Display Interface** | 4-wire SPI (`SPI2_HOST`), `20 MHz` write / `16 MHz` read | Verified (`cfg.spi_3wire = false`, `readable = false`) |
| **Touch Controller IC** | `XPT2046` resistive touch controller | Verified (`lgfx::Touch_XPT2046`) |
| **Touch Interface** | Shared `SPI2_HOST` @ `1.5 MHz`, polled (`pin_int = -1`) | Verified (`bus_shared = true`) |
| **Backlight Control** | Tied to `3.3V` (software RGB565 dimming) OR wired to `GPIO 14` (hardware PWM) | Verified in code comment (line 123); actual physical wire is **unverified** |

> **Note on ILI9488 Color Depth over SPI:** Over 4-wire SPI, the `ILI9488` controller expects 18-bit (`RGB666`, 3 bytes/pixel) on the wire. `LovyanGFX` transparently converts the 16-bit (`RGB565`) PSRAM `canvas` sprite to SPI transactions during `pushSprite()`.

---

## 3. Connected Peripherals & I/O

| Peripheral | Pin | Electrical Range / Behavior | Verification |
|---|---|---|---|
| **ADC Oscilloscope Input** | `GPIO 4` | 12-bit ADC (`0..4095`), scaled to `0.00V .. 3.30V` | Verified in code; external probe circuit **unverified** |
| **Digital Output 1** | `GPIO 5` | `OUTPUT`, `LOW` (0V) at boot, toggled via UI | Verified in code; external load **unverified** |
| **Digital Output 2** | `GPIO 6` | `OUTPUT`, `LOW` (0V) at boot, toggled via UI | Verified in code; external load **unverified** |
| **Digital Output 3** | `GPIO 7` | `OUTPUT`, `LOW` (0V) at boot, toggled via UI | Verified in code; external load **unverified** |
| **Backlight PWM (Optional)** | `GPIO 14` | `analogWrite()` duty `35..255` | Verified in code; physical jumper **unverified** |
| **Onboard WS2812 RGB** | `GPIO 48` | `neopixelWrite()` / `rgbLedWrite()`, scaled to 25% max brightness | Verified in code |

---

## 4. Power & Electrical Safety Notes (Important for Beginners)
1. **3.3 V Logic Only:** All ESP32-S3 GPIOs (`GPIO 4, 5, 6, 7, 14, 38–42, 1, 3, 48`) are strictly **3.3 V**. Never feed `5 V` directly into `GPIO 4` (the ADC Oscilloscope pin) or you can permanently damage the chip.
2. **Backlight Current:** A 3.5" TFT backlight draws ~60–120 mA. If you wire the display `LED` pin to `GPIO 14` instead of `3.3V`, use a transistor/MOSFET driver or verify your module's `LED` pin already has an onboard transistor—do not exceed ESP32-S3 GPIO max drive current (~20–40 mA).
3. **USB Power:** Wi-Fi transmission peaks + TFT backlight + 8 MB OPI PSRAM require a clean 5 V USB supply (use a good quality data cable and direct motherboard/powered hub USB port).
4. **SD Card Slot (on 14-pin Red PCB):** Many 14-pin red ILI9488 PCBs include a rear SD card header (`SD_CS`, `SD_MOSI`, `SD_MISO`, `SD_SCK`). It is **not used** in `AURAOS.ino`.
