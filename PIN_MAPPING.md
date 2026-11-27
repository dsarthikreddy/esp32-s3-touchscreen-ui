# PIN_MAPPING.md — ESP32-S3 GPIO & 14-Pin Display Wiring

All pin numbers below are **verified from source code** ([cuteOS.ino](cuteOS.ino), lines 48–137). Physical jumper wires on the user's desk were not visually inspected; verify against your physical breadboard/PCB before powering on.

---

## 1. Master GPIO Allocation Table (ESP32-S3-N16R8)

| ESP32-S3 GPIO | Firmware Symbol | Subsystem | Signal / Direction | Notes |
|---|---|---|---|---|
| **GPIO 1** | `cfg.pin_cs` (Panel) | ILI9488 Display | `LCD_CS` (Output, Active Low) | Display Chip Select |
| **GPIO 3** | `cfg.pin_rst` (Panel) | ILI9488 Display | `LCD_RST` (Output) | Hardware Reset (also strapping pin on ESP32-S3—see §3) |
| **GPIO 4** | `ADC_SCOPE_PIN` | ADC Oscilloscope | Analog Input (`ADC1_CH3`) | 12-bit (`0–4095`), max **3.3 V** |
| **GPIO 5** | `OUT_GPIOS[0]` | RGB & GPIO Studio | Digital Output (`LOW` at boot) | User-toggled 0V / 3.3V |
| **GPIO 6** | `OUT_GPIOS[1]` | RGB & GPIO Studio | Digital Output (`LOW` at boot) | User-toggled 0V / 3.3V |
| **GPIO 7** | `OUT_GPIOS[2]` | RGB & GPIO Studio | Digital Output (`LOW` at boot) | User-toggled 0V / 3.3V |
| **GPIO 14** | `BACKLIGHT_PWM_PIN` | Display Backlight | PWM Output (`analogWrite`) | Optional hardware PWM; if display `LED` is tied to `3V3`, software dimming still works |
| **GPIO 38** | `cfg.pin_miso` | Shared `SPI2_HOST` | `MISO` (Input) | Shared by ILI9488 config & XPT2046 `T_DO` |
| **GPIO 39** | `cfg.pin_mosi` | Shared `SPI2_HOST` | `MOSI` (Output) | Shared by ILI9488 `SDI` & XPT2046 `T_DIN` |
| **GPIO 40** | `cfg.pin_sclk` | Shared `SPI2_HOST` | `SCLK` (Output) | Shared by ILI9488 `SCK` & XPT2046 `T_CLK` |
| **GPIO 41** | `cfg.pin_dc` | ILI9488 Display | `DC / RS` (Output) | Data/Command select |
| **GPIO 42** | `cfg.pin_cs` (Touch) | XPT2046 Touch | `T_CS` (Output, Active Low) | Touch Chip Select (`MTMS` JTAG pin; fine once JTAG disabled) |
| **GPIO 48** | `ONBOARD_RGB_PIN` | Onboard RGB LED | WS2812 Data (Output) | Built-in addressable LED on ESP32-S3 DevKit |
| **-1 (Unwired)** | `cfg.pin_busy`, `cfg.pin_int` | Panel Busy / Touch IRQ | Not connected | Touch uses SPI polling instead of IRQ |

---

## 2. 14-Pin Red PCB ILI9488 + XPT2046 Header Mapping

Standard 14-pin red SPI TFT + Touch modules expose pins 1–14 in the order below (verify silkscreen labels on your exact PCB):

| Header Pin | Silkscreen Name | Connected ESP32-S3 Pin | Verification |
|---|---|---|---|
| 1 | `VCC` | `3.3V` or `5V` (per module regulator) | Assumed (check J1 regulator on PCB; ESP32-S3 logic is 3.3V) |
| 2 | `GND` | `GND` | Assumed |
| 3 | `CS` | **GPIO 1** | Verified in code |
| 4 | `RESET` | **GPIO 3** | Verified in code |
| 5 | `DC/RS` | **GPIO 41** | Verified in code |
| 6 | `SDI (MOSI)` | **GPIO 39** | Verified in code |
| 7 | `SCK` | **GPIO 40** | Verified in code |
| 8 | `LED` | `3.3V` (always on) **or** **GPIO 14** (PWM) | Comment line 123 states `3V3` or GPIO 14 |
| 9 | `SDO (MISO)` | **GPIO 38** (optional for display since `readable=false`) | Verified in code |
| 10 | `T_CLK` | **GPIO 40** (shared with `SCK`) | Verified in code |
| 11 | `T_CS` | **GPIO 42** | Verified in code |
| 12 | `T_DIN` | **GPIO 39** (shared with `MOSI`) | Verified in code |
| 13 | `T_DO` | **GPIO 38** (shared with `MISO`) | Verified in code |
| 14 | `T_IRQ` | Not connected (`-1`) | Verified in code |

---

## 3. ESP32-S3 Pin Safety & Reserved Pins Avoided

- **OPI PSRAM & Flash Reserved Pins (`GPIO 26–37`):** On ESP32-S3-N16R8 modules with Octal PSRAM, `GPIO 33–37` (in addition to SPI0/1 `26–32`) are internally used by Octal PSRAM/Flash and **must never be used**. `cuteOS.ino` uses `GPIO 38–42`, which safely avoids `26–37`.
- **Strapping Pins (`GPIO 0, 3, 45, 46`):**
  - `GPIO 3` is used as `LCD_RST`. On ESP32-S3, `GPIO 3` selects JTAG signal source if eFuse is unburned, though in default hardware CDC mode it operates safely as a push-pull output after boot. Avoid external pull-downs that fight boot strapping.
- **USB Pins (`GPIO 19, 20`):** Untouched (`USB D-` / `USB D+`), keeping USB CDC/JTAG functional.
- **UART0 Pins (`GPIO 43, 44`):** Untouched, keeping `Serial` at `115200` baud available.
