# DECISIONS.md — Architecture Decision Records (ADRs)

---

## ADR-001: Single-File Sketch (`AURAOS.ino`) Preserved Over Multi-File Split
- **Context:** All firmware logic (~2,240 lines across 15 sections) resides in [AURAOS.ino](AURAOS.ino).
- **Decision:** Keep the single-file structure intact rather than splitting into `.h`/`.cpp` modules during documentation and repository setup.
- **Rationale:** Zero risk of breaking Arduino IDE prototype generation, global symbol ordering, or working behavior; trivially inspectable in one file by beginners and AI assistants.
- **Consequences:** Longer single file (~96 KB); future feature growth (e.g., LVGL migration) will merit splitting into `ui/`, `net/`, `hal/` modules.

---

## ADR-002: Direct `LovyanGFX` PSRAM Sprite Composition Instead of LVGL
- **Context:** The project needed custom frosted glass cards, rounded squircles, live map tile stitching, scaled JPEG/PNG thumbnails, and fast boot splash graphics.
- **Decision:** Use `LovyanGFX` `1.3.0` with 5 full-screen `LGFX_Sprite` buffers in OPI PSRAM rather than LVGL.
- **Rationale:** Simple zero-boilerplate setup (1 external library), full control over pixel blending (`blend565`), and pre-baked static glass buffers.
- **Consequences:** UI layout coordinates and hitboxes (`inRect`) are manually coded. See [ARCHITECTURE.md](ARCHITECTURE.md) §6 for LVGL migration assessment.

---

## ADR-003: Pre-Baked Liquid Glass Buffers (`frostSprite` & `darkGlass`)
- **Context:** Computing per-pixel box/Gaussian blur over 480x320 RGB565 every frame is too slow on the CPU.
- **Decision:** Pre-compute `frostSprite` and `darkGlass` in OPI PSRAM whenever `bgSprite` changes (`rebuildGlassBuffers()`), then blit clipped regions during screen draws.
- **Rationale:** O(1) fast card rendering with smooth translucent visual depth at the cost of 600 KB PSRAM (out of 8 MB available).

---

## ADR-004: Dual Software + Optional Hardware Backlight Dimming
- **Context:** Many 14-pin red PCB ILI9488 modules have their `LED` pin tied directly to `3.3V` on breadboards.
- **Decision:** Scale RGB565 pixels inside `canvas` (`applySoftwareBrightnessRect`) **and** output PWM on `GPIO 14` (`applyHardwarePWM`).
- **Rationale:** Brightness slider (`20%–100%`) visibly dims the UI even without a hardware backlight wire, while supporting true LED PWM if `GPIO 14` is wired.

---

## ADR-005: Shared `SPI2_HOST` Bus for `ILI9488` and `XPT2046`
- **Context:** Reduces wiring pin count on the 14-pin display header (`SCK`, `MOSI`, `MISO` shared).
- **Decision:** Enable `cfg.bus_shared = true` and `cfg.use_lock = true` on `SPI2_HOST`, with conservative clocks (`20 MHz` display, `1.5 MHz` touch).
- **Rationale:** Prevents SPI bus contention between display DMA transfers and touch polling.

---

## ADR-006: `WiFiClientSecure::setInsecure()` for Public HTTPS Feeds
- **Context:** Firmware fetches from Wikimedia, BBC, OpenStreetMap, Esri, and Open-Meteo across different certificate authorities.
- **Decision:** Use `client.setInsecure()` instead of embedding and maintaining a multi-CA root bundle.
- **Rationale:** Eliminates certificate expiration failures on an offline-capable hobby device; acceptable because all endpoints are public read-only feeds with no user credentials transmitted.
- **Consequences:** Must NOT be reused if future features send passwords, OAuth tokens, or private user data over HTTPS.

---

## ADR-007: Secrets Extraction via `secrets.h` + `secrets.example.h`
- **Context:** Original `AURAOS.ino` embedded Wi-Fi SSID/password and exact residential coordinates/estate labels directly in source code.
- **Decision:** Extract all credentials and private location strings into gitignored `secrets.h`, commit sanitized `secrets.example.h`, and include via `#if __has_include("secrets.h")` fallback.
- **Rationale:** Preserves 100% identical local build/runtime behavior for the owner while ensuring zero secrets or PII are pushed to GitHub and allowing fresh clones to compile out of the box.
