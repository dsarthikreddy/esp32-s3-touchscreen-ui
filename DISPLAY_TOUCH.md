# DISPLAY_TOUCH.md — Display, Touch, Calibration & Brightness

All driver settings documented below are **verified directly from [AURAOS.ino](AURAOS.ino)** using `LovyanGFX` `1.3.0`.

---

## 1. LovyanGFX Driver Configuration (`class LGFX`)

### Shared SPI Bus (`lgfx::Bus_SPI`)
- **Host:** `SPI2_HOST` (`FSPI` on ESP32-S3)
- **Mode:** SPI Mode `0` (`CPOL=0, CPHA=0`)
- **Clock Rates:** `freq_write = 20,000,000` (20 MHz), `freq_read = 16,000,000` (16 MHz)
- **3-Wire:** `false` (standard 4-wire SPI with separate `DC`, `MOSI`, `MISO`)
- **Bus Locking & DMA:** `use_lock = true`, `dma_channel = SPI_DMA_CH_AUTO`

### Display Panel (`lgfx::Panel_ILI9488`)
- **Geometry:** `panel_width = 320`, `panel_height = 480`, `offset_x = 0`, `offset_y = 0`, `offset_rotation = 0`
- **Runtime Rotation:** `tft.setRotation(1)` -> **480 x 320 landscape**
- **SPI Flags:** `readable = false`, `invert = false`, `rgb_order = false`, `dlen_16bit = false`, `bus_shared = true`
- **Dummy Read:** `dummy_read_pixel = 8`, `dummy_read_bits = 1`

> **Why `20 MHz`?** On breadboard jumper wires connecting a 14-pin red PCB ILI9488 + XPT2046, `20 MHz` provides reliable signal integrity without SPI corruption, whereas `40–80 MHz` often causes pixel snow or touch failures over long Dupont wires.

---

## 2. Touch Controller (`lgfx::Touch_XPT2046`)

- **SPI Sharing:** `spi_host = SPI2_HOST`, `bus_shared = true`, `cs = 42`, `freq = 1,500,000` (1.5 MHz)
- **IRQ Pin:** `pin_int = -1` (polled every `loop()` iteration)
- **Raw Default Bounds:** `x_min = 300`, `x_max = 3900`, `y_min = 200`, `y_max = 3900`, `offset_rotation = 0`

### Multi-Stage Touch Filtering (`getFilteredTouch` & `pollTouch`)
1. **3-Sample Median Filter (`getFilteredTouch`):** Reads up to 3 samples spaced `70 us` apart. If 3 samples succeed, returns the median `(x, y)` per axis to reject resistive contact bounce/spikes.
2. **Exponential Moving Average (`pollTouch`):** While finger/stylus remains down, applies `smooth = smooth * 0.4f + raw * 0.6f`.
3. **Touch-Up Debounce:** Releases `touchActive` only after `millis() - lastTouchSeen > 18 ms`, preventing micro-drops mid-swipe.
4. **Gesture Discrimination:**
   - If `abs(dx) > 38 || abs(dy) > 38` pixels -> `handleSwipe(dx, dy)`
   - Otherwise -> `handleTap(touchStartX, touchStartY)`
5. **Visual Feedback Ring:** Draws a `9 px` cyan circle (`C_CYAN`) on initial touch-down and restores the dirty `22x22` patch via `pushPartial()` on release.

---

## 3. Persistent Touch Calibration (NVS Flash)

- **Storage:** ESP32 `Preferences` library, namespace `"cuteos"`, key `"cal"` (`uint16_t calData[8]`, 16 bytes).
- **Boot Flow (`setup()`):**
  - If `"cal"` exists in NVS (`prefs.getBytesLength("cal") == 16`), loads it immediately via `tft.setTouchCalibrate(savedCal)`.
  - If missing (first boot or after NVS erase), invokes `runAndSaveTouchCalibration()` which calls `tft.calibrateTouch(calData, C_BLUE, C_WHITE, 18)` and saves the 8-element matrix to NVS.
- **Re-Calibration:** Available anytime from **Control Center -> `Calib Touch`** tile.

---

## 4. Dual Brightness Control (`20%` – `100%`)

`AURAOS.ino` implements two simultaneous dimming mechanisms so brightness works regardless of how pin 8 (`LED`) on the display is wired:

1. **Software Brightness (`applySoftwareBrightnessRect`):**
   - Scales every RGB565 pixel inside `canvas` buffer just before pushing to the screen (`pushFullFrame()` / `pushPartial()`), when `screenBrightness < 98`.
   - Automatically detects `LovyanGFX` byte-swap state at boot (`spriteIsSwapped = (((uint16_t*)canvas.getBuffer())[0] == 0x00F8)`) so RGB channel scaling works without color corruption.
   - **Advantage:** Works even when the display `LED` pin is tied directly to `3.3V`.
2. **Hardware PWM (`applyHardwarePWM`):**
   - Drives `GPIO 14` (`BACKLIGHT_PWM_PIN`) with `analogWrite(14, constrain((screenBrightness * 255) / 100, 35, 255))`.
   - Active if the user moves the display `LED` pin to `GPIO 14` (via an appropriate driver/transistor).
3. **Persistence:** Saved to NVS (`"cuteos"` / `"bri"`) on release of the Control Center brightness slider.
