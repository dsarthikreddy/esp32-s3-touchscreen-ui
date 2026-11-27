# BUGS.md — Known Issues, Edge Cases & Technical Debt

All items below were identified through static inspection of [cuteOS.ino](cuteOS.ino). Existing behavior was intentionally preserved without redesign.

---

## High / Functional Limitations

1. **Synchronous Blocking Network Calls (`downloadBufferHTTPS`, `fetchLocalWeather`):**
   - HTTP/TLS downloads run on the main Arduino `loop()` / `setup()` thread with timeouts of `5000–6500 ms`.
   - **Impact:** Touch input freezes during manual News refresh, Map Street/Satellite switch, Weather refresh (including automatic 15-minute background refresh in `loop()`), or Everest download.
2. **TLS Certificate Verification Disabled (`client.setInsecure()`):**
   - `downloadBufferHTTPS()` and `fetchLocalWeather()` skip TLS root CA validation.
   - **Impact:** Vulnerable to MITM on untrusted networks; do not add authenticated/private API calls without proper root CA pinning or `WiFiClientSecure::setCACertBundle()`.
3. **Top 4 RF Radar SSIDs Not Sorted by Signal Strength (`runRFSpectrumScan`):**
   - Lines 1733–1737 store `i < 4` directly from `WiFi.scanNetworks()` order. While ESP-IDF often returns networks sorted by RSSI, it is not guaranteed across all driver versions.
4. **Hardcoded `C_DARK_PILL` Text Contrast Assumption on News Cards (`SCR_NEWS`):**
   - Lines 1249–1251 blend `C_DARK_PILL` into the bottom of top news images via `canvas.readPixel()` before drawing white text, which works, though very fast partial refreshes don't exist here and thumbnail aspect ratios (`240x135` -> `220x114` or `142x66`) use nearest-neighbor integer scaling (`blitScaledNewsImg`), causing slight pixel aliasing on small cards.

---

## Medium / Edge Cases

5. **Calculator Divide-by-Zero Returns `0.0` Silently (`handleCalcKey`):**
   - Line 1821: `if (calcOp == '/') res = (b != 0.0) ? (calcAcc / b) : 0.0;` displays `0` instead of an `Error` / `NaN` message.
   - Chained operations without pressing `=` (e.g., `2 + 3 + 4`) overwrite `calcAcc` with `3` instead of evaluating `2 + 3` first.
6. **Hardware PWM Backlight (`GPIO 14`) Requires External Wire / Driver:**
   - Software brightness works down to `20%`, but `20%` RGB565 scaling noticeably reduces color bit-depth (quantization banding in dark gradients) if hardware backlight remains at 100% on `3.3V`.
7. **Software Brightness Double-Scaling Risk on `pushPartial()`? (Checked — Safe, Minor Quirk):**
   - `canvas` is overwritten before `pushFullFrame()` on full renders, so scaling `canvas` in place is fine. However, during `APP_SCOPE` (`drawScopeBodyOnly()` + `pushPartial()`), `drawScopeBodyOnly()` redraws the scope card fresh before `applySoftwareBrightnessRect()` scales only `(14, 44, 452, 266)`—meaning repeated `pushPartial()` calls on static areas don't double-dim, though the outer border/wallpaper outside `(14, 44, 452, 266)` retains the brightness from the previous `pushFullFrame()`.
8. **Offline Clock Fallback Starts at `12:00` (`drawHomeHeroCard`):**
   - When `ntpSynced == false`, line 1183 computes `12 + (s / 3600) % 12`, which cycles `12:xx`..`23:xx` and wraps back to `12:00` instead of `00:00`..`23:59`.
9. **`boot` vs `Control Center` `Wi-Fi` Tile Does Not Update `mapLoaded` / `newsLoaded`:**
   - If the device boots offline and the user later reconnects via **Control Center -> Wi-Fi**, it syncs NTP and Weather, but does not automatically fetch Everest wallpaper, Map tiles, or Live News (user must tap `Everest BG`, `Map`, or `News -> Refresh`).
10. **Map Tile Download Uses Sequential HTTPS Connections (`downloadAreaRealMap`):**
    - Fetches up to 9 separate tiles (`3x3`) sequentially over fresh TLS handshakes; on slower Wi-Fi this can take several seconds and occasionally hit the 6.5 s timeout on individual tiles, leaving partial background rectangles if one tile fails.

---

## Low / Code Cleanliness

11. **`ESP32` Core 3.x Untested:** Only compiled and verified against `esp32:esp32` `2.0.17`.
12. **No `Wire.h` / Touch IRQ Optimization:** `pin_int = -1` polls SPI2 at 1.5 MHz continuously in `loop()`, adding mild SPI bus traffic even when idle.
