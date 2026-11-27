# UI_DESIGN.md — Visual System, Screens, Widgets & Navigation

`cuteOS v2.0` renders a bright, iOS-inspired "liquid glass" interface at **480 x 320 landscape** in 16-bit RGB565 color.

---

## 1. Visual Language & Design Tokens

### Color Palette (`AURAOS.ino` lines 147–175)
- **Neutrals & Glass:** `C_WHITE` (`0xFFFF`), `C_SNOW` (`0xF7DF`), `C_SOFT_GRAY` (`0xE75E`), `C_BORDER` (`0xD6DC`), `C_INK` (`0x18E4`), `C_INK_SUB` (`0x5B2E`), `C_DARK_PILL` (`0x10A4`)
- **Vibrant Accents:** `C_BLUE` (`0x13DF`), `C_CYAN` (`0x0E7F`), `C_GREEN` (`0x262B`), `C_GOLD` (`0xFE21`), `C_ORANGE` (`0xFBAA`), `C_RED` (`0xF9CA`), `C_ROSE` (`0xF96E`), `C_VIOLET` (`0x9A1E`), `C_TEAL` (`0x05D9`)
- **Soft Card Tints:** `C_TINT_BLUE`, `C_TINT_GREEN`, `C_TINT_GOLD`, `C_TINT_ROSE`, `C_TINT_VIOLET`

### Typography
Uses LovyanGFX built-in serif fonts across all UI screens:
- `fonts::FreeSerif9pt7b` / `FreeSerifBold9pt7b` (labels, metadata, pills, wrapped article text)
- `fonts::FreeSerifBold12pt7b` (card headers, calculator keys, Dynamic Island expanded title)
- `fonts::FreeSerifBold18pt7b` (weather widget temp, calculator display, calibration header)
- `fonts::FreeSerifBold24pt7b` (boot logo, home clock `HH:MM`, weather hero temp, timer countdown)

### Pre-Baked Liquid Glass Pipeline
Instead of performing expensive real-time Gaussian blurs on every frame, `rebuildGlassBuffers()` samples `bgSprite` in 4x4 blocks and pre-computes:
- `frostSprite`: Light frosted glass (`blend565(C_WHITE, blur, 198)`)
- `darkGlass`: Dark frosted glass (`blend565(C_DARK_PILL, blur, 168)`)

`drawFrostGlassCard()` clips `canvas` to the card rectangle, blits `frostSprite` or `darkGlass`, restores outer rounded corners via `restoreRoundedCorners()`, and draws a 1 px specular top rim (`C_WHITE`).

---

## 2. Screen Inventory (14 Screens)

| ID | Macro | Screen Name | Entry Method | Primary Features |
|---|---|---|---|---|
| `0` | `SCR_HOME` | Home Screen | Boot default / `< Home` | Dynamic Island, frosted clock/location hero card, 2 customizable widget slots, 5-icon squircle dock |
| `1` | `SCR_NEWS` | Live News Feed | Swipe Left->Right / Dock News / Island | 5 topic tabs (`Top`, `India`, `Tech`, `Science`, `Sport`), 2 large photo cards + 3 compact photo cards |
| `2` | `SCR_ARTICLE` | Article Reader | Tap any news tile | Scaled image, topic badge, wrapped title & summary, pubDate, `< Prev` / `Next >` |
| `3` | `SCR_CONTROL` | Control Center | Swipe Down / Dock Control / Island | Live brightness slider (`20–100%`), 8 quick toggles (`Wi-Fi`, `Everest BG`, `Map`, `RGB Lamp`, `RF Scan`, `Calib Touch`, `GPIO Reset`, `Reboot OS`) |
| `4` | `SCR_APPS` | App Drawer | Swipe Up / Dock Apps | Frosted 4x2 grid of squircle icons (`Live News`, `Estate Map`, `Weather`, `RF Radar`, `Scope`, `RGB & IO`, `Calculator`, `Timer`) |
| `5` | `SCR_TILES` | Tech Tiles & Notifs | Swipe Right->Left | 4 swipeable pages: (1) Map + Weather, (2) 2.4 GHz RF spectrum, (3) ESP32-S3 core stats + GPIO 5/6/7 toggles, (4) Notification center |
| `6` | `SCR_WIDGETS` | Widget Customizer | Tap `+ Widgets` | Assign any of 7 widgets (`W_WEATHER`..`W_CORE`) to Top (`homeSlot[0]`) or Bottom (`homeSlot[1]`), saved to NVS |
| `7` | `APP_MAP` | Local Area Map | Hero card / Dock Map / Apps | Full-screen Zoom-17 OSM street or Esri satellite tiles with center pin + Street/Satellite toggle |
| `8` | `APP_WEATHER` | Weather & Forecast | Dock Weather / Apps / Weather widget | Current temp, humidity, wind, WMO icon + 3-day forecast cards |
| `9` | `APP_RADAR` | Wi-Fi RF Radar | Apps / Radar widget | 13-channel 2.4 GHz spectrum bar chart + top 4 SSIDs with channel & dBm |
| `10` | `APP_SCOPE` | ADC Oscilloscope | Apps | Live 80-point `GPIO 4` waveform (`0–3.3V`, ~18 Hz frame update), `Hold`/`Resume`, internal core temp |
| `11` | `APP_RGB` | RGB & GPIO Studio | Apps | 7 onboard WS2812 color presets (`GPIO 48`), `Breathe`/`Solid` toggle, 3 `GPIO 5/6/7` output toggles |
| `12` | `APP_CALC` | Calculator | Apps | 4x4 grid (`0–9`, `.`, `+`, `-`, `*`, `/`, `=`) + header `Clear`, partial display redraw |
| `13` | `APP_TIMER` | Focus Timer | Apps / Timer widget | Circular progress ring, `START`/`PAUSE`, `Reset`, `5 min`, `15 min`, `25 min Pomodoro` |

---

## 3. Modular Home Widgets (`homeSlot[0]` & `homeSlot[1]`)

| ID | Macro | Title | Tap Action |
|---|---|---|---|
| `0` | `W_WEATHER` | Weather | Opens `APP_WEATHER` |
| `1` | `W_MINIMAP` | Area Map | Opens `APP_MAP` |
| `2` | `W_NEWS` | Live News | Opens `SCR_NEWS` |
| `3` | `W_RADAR` | RF Radar | Opens `APP_RADAR` |
| `4` | `W_GPIO` | GPIO 5-7 | Toggles `GPIO 5`, `6`, or `7` directly on Home Screen |
| `5` | `W_TIMER` | Timer | Opens `APP_TIMER` |
| `6` | `W_CORE` | ESP32 Core | Opens `SCR_CONTROL` |

---

## 4. Dynamic Island States
- **Collapsed (`206 x 28` px at `x=137, y=6`):** Shows connection dot (`C_GREEN` online / `C_GOLD` offline), `"cuteOS"`, and either active timer countdown (`MM:SS`) or current temperature (`XX C`).
- **Expanded (`356 x 84` px at `x=62, y=6`):** Shows `"cuteOS Live Hub"`, Wi-Fi status, and 4 quick-action pills: `News`, `Map`, `Bright` (opens Control Center), and `Close`.
