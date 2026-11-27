# ARCHITECTURE.md — Software Architecture & Future LVGL Assessment

---

## 1. High-Level System Architecture

`cuteOS.ino` is structured as a **single-translation-unit, retained-canvas immediate-mode GUI** running on Arduino-ESP32 (`loop()` on Core 1, Wi-Fi/TCP stack on Core 0).

```text
+-------------------------------------------------------------------+
|                         cuteOS.ino (loop)                         |
|  pollTouch() ---> handleSwipe() / handleTap() ---> uiDirty = true |
|  ADC Scope (55ms) | RGB Breathe (45ms) | Timer (1s) | NTP (10s)   |
+---------------------------------+---------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                    Composition Pipeline (PSRAM)                   |
|  bgSprite / frostSprite / darkGlass / mapSprite / newsImg[0..4]   |
|                         ---> canvas (480x320 RGB565)              |
|                         ---> applySoftwareBrightnessRect()        |
|                         ---> tft.pushSprite() / pushPartial()     |
+---------------------------------+---------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|               LovyanGFX 1.3.0 (SPI2_HOST w/ DMA & Lock)           |
|      Panel_ILI9488 (CS=1, DC=41)  |  Touch_XPT2046 (CS=42)        |
+-------------------------------------------------------------------+
```

---

## 2. OPI PSRAM Memory Budget

Every full-screen sprite is 16-bit RGB565 (`480 * 320 * 2 = 307,200` bytes = **300 KB**):

| Buffer | Dimensions | Color Depth | Location | Size |
|---|---|---|---|---|
| `bgSprite` | 480 x 320 | 16-bit | OPI PSRAM | 300 KB |
| `frostSprite` | 480 x 320 | 16-bit | OPI PSRAM | 300 KB |
| `darkGlass` | 480 x 320 | 16-bit | OPI PSRAM | 300 KB |
| `mapSprite` | 480 x 320 | 16-bit | OPI PSRAM | 300 KB |
| `canvas` | 480 x 320 | 16-bit | OPI PSRAM | 300 KB |
| `newsImg[0..4]` (x5) | 240 x 135 | 16-bit | OPI PSRAM | 5 x 64.8 KB = 324 KB |
| **Total Persistent Sprites** | — | — | **OPI PSRAM** | **~1.82 MB** (of 8 MB) |

Transient HTTPS downloads (`downloadBufferHTTPS`) also allocate from PSRAM via `ps_malloc()` (up to 160 KB for Everest JPEG, 120 KB per map tile, 70 KB per news image, 48 KB for RSS XML) and `free()` immediately after decoding.

Compiled static memory footprint (see [TEST_RESULTS.md](TEST_RESULTS.md)):
- **Program Flash:** `1,071,321` bytes (`34%` of 3 MB `huge_app` slot)
- **Static SRAM Globals:** `50,756` bytes (`15%` of 327,680 bytes)

---

## 3. Boot Sequence (`setup()`)
1. Initialize Serial (`115200`), 12-bit ADC, `GPIO 14` PWM, `GPIO 5/6/7` LOW, and `GPIO 48` WS2812 blue.
2. `tft.init()` + `tft.setRotation(1)` (landscape 480x320).
3. Load NVS `"cuteos"` (`bri`, `w0`, `w1`, `cal`) and apply brightness.
4. Draw initial boot splash directly to `tft`.
5. Allocate 5 full-screen + 5 thumbnail sprites in OPI PSRAM; halt on failure.
6. Detect RGB565 endianness (`spriteIsSwapped`) via `canvas.drawPixel(0, 0, TFT_RED)`.
7. Generate procedural Everest fallback + pre-baked glass + offline map + offline news placeholders.
8. Apply NVS touch calibration or launch interactive 4-corner calibration.
9. Draw Home with local artwork. Start Wi-Fi and resume a previously paired phone after the first frame.
10. Fetch weather and the wallpaper on a network task. Load news when its screen opens and map tiles when requested in Map.

---

## 4. Runtime Loop (`loop()`) & Partial Updates
- **Touch (`pollTouch`):** Runs every ~2 ms iteration; handles swipe/tap dispatch and live slider dragging.
- **Oscilloscope (`APP_SCOPE`):** Samples `GPIO 4` every `55 ms` (~18 fps) and uses `pushPartial(14, 44, 452, 266)` to avoid redrawing the header.
- **RGB Breathing:** Updates WS2812 sine wave every `45 ms` when `rgbBreathing == true`.
- **Focus Timer:** 1-second tick updates `timerRemainSec` and flags `uiDirty` on `APP_TIMER` / `SCR_HOME`.
- **Clock & Weather:** Checks NTP every `10 s` (partial redraw of hero card on `SCR_HOME`), refreshes weather every `15 min` (`900,000 ms`).

---

## 5. Security & Persistence Model
- **NVS (`Preferences`):** Stores only non-secret UI settings (`bri`, `w0`, `w1`, `cal`) in flash namespace `"cuteos"`.
- **Secrets:** Managed at compile time via `secrets.h` (gitignored), with `secrets.example.h` fallback.
- **HTTPS:** Uses `WiFiClientSecure::setInsecure()` for public read-only endpoints (no user credentials or tokens sent over HTTP).

---

## 6. Future LVGL Integration Assessment (Without Changing Existing Firmware)

> This section evaluates migrating `cuteOS` to **LVGL (v8.3 or v9.x)** in a future branch while keeping the current LovyanGFX firmware untouched.

### Feasibility: **High (Well-Suited Hardware)**
The ESP32-S3-N16R8 has **8 MB OPI PSRAM**, **16 MB Flash**, and **DMA SPI2**, which easily exceeds LVGL's minimum requirements (~64 KB RAM + 200 KB Flash).

### Strengths vs. Current LovyanGFX Sprite Architecture
| Aspect | Current (`LovyanGFX` Direct Sprites) | Future (`LVGL v8/v9` + `LovyanGFX` Flush) |
|---|---|---|
| **Animations & Transitions** | Manual frame redraws, instant screen swaps | Built-in smooth screen slide/fade animations, spring physics, scroll inertia |
| **Widgets & Layouts** | Hand-coded `inRect()` hitboxes and `drawWrappedText` | Native flex/grid layouts, scrollable lists, keyboards, charts, sliders, arcs |
| **Memory & Bandwidth** | 5 full 300 KB PSRAM screens + full-frame `pushSprite` | Single/double partial draw buffer in internal DMA SRAM (e.g. `480 x 40` px = `38.4 KB`) -> faster SPI flush |
| **Blur / Glass Effects** | Pre-baked `frostSprite` (fast, zero runtime blur cost) | Real-time LVGL blur is CPU-heavy on ESP32-S3; **keep pre-baked glass image or semi-transparent overlay** |
| **Code Complexity** | Self-contained 1-file `.ino`, 1 library (`LovyanGFX`) | Requires `lv_conf.h`, tick/timer management, thread/mutex safety if using FreeRTOS tasks, multi-file refactor |

### Recommended LVGL Integration Strategy (When Ready)
1. **Keep `class LGFX` as the Hardware Abstraction Layer (HAL):** LVGL does not replace `LovyanGFX`; it uses `tft.pushImageDMA()` inside `my_disp_flush()` and `getFilteredTouch()` inside `my_touchpad_read()`.
2. **Use Internal SRAM for LVGL Draw Buffer:** Allocate two `480 x 40` RGB565 buffers in **internal DMA-capable SRAM** (`heap_caps_malloc(..., MALLOC_CAP_DMA)`) rather than PSRAM for fastest SPI throughput, while keeping decoded JPEG/PNG wallpapers, map sprites, and news thumbnails in OPI PSRAM (`lv_img_dsc_t`).
3. **Offload Network Fetches to FreeRTOS Task (Core 0):** Currently `downloadBufferHTTPS` blocks `loop()`. Moving HTTP/TLS fetches to a Core 0 FreeRTOS worker task (passing results via queue/mutex to LVGL on Core 1) will keep LVGL animations at 30–60 fps even while downloading news/maps.
4. **Software Brightness:** Note that LVGL partial flush buffers mean a post-flush RGB565 scale hook (or `GPIO 14` hardware PWM backlight) should replace `applySoftwareBrightnessRect()`.
5. **Recommended Branching:** Create a separate `feat/lvgl-migration` branch rather than modifying `main` until all 14 screens reach parity.
