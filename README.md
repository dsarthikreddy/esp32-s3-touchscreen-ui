<div align="center">

# cuteOS

**A glass-inspired touchscreen interface for the ESP32-S3**

480 × 320 landscape · ILI9488 display · XPT2046 resistive touch · Arduino + LovyanGFX

[Open the firmware](cuteOS.ino) · [Wire the display](#1-wire-the-display-and-touch) · [Build and upload](#2-build-and-upload) · [Troubleshoot](#troubleshooting)

</div>

---

## 1. Wire the display and touch

**Hardware:** ESP32-S3-N16R8 development board (16 MB flash, 8 MB Octal PSRAM) and a 3.5-inch **SPI** ILI9488 display with an XPT2046 touch controller. Use a USB data cable and short jumper wires.

Disconnect USB power before wiring. Match the **labels printed on your module** and use GPIO numbers on the ESP32-S3. Header order can vary between boards.

| Display module label | ESP32-S3 connection | What it does |
|---|---|---|
| `VCC` | `3V3` if supported by the module | Display power. Check your board's rated supply before connecting it. |
| `GND` | `GND` | Common ground |
| `CS` | **GPIO 1** | Selects the display |
| `RESET` / `RST` | **GPIO 3** | Resets the display |
| `DC` / `RS` | **GPIO 41** | Selects data or command |
| `SDI` / `MOSI` **and** `T_DIN` | **GPIO 39** | Shared SPI data from the ESP32-S3 |
| `SCK` **and** `T_CLK` | **GPIO 40** | Shared SPI clock |
| `T_CS` | **GPIO 42** | Selects the touch controller |
| `T_DO` | **GPIO 38** | SPI data from the touch controller |
| `LED` / `BL` | Module-rated backlight supply or driver | For a module with a 3.3 V-rated LED input, use `3V3`. The firmware can dim the image in software. |
| `SDO` / `MISO` and `T_IRQ` | Leave open | The firmware writes to the display and polls touch over SPI. |

**Before powering on:** Check that `CS` and `T_CS` go to separate GPIOs, `T_DO` goes to GPIO 38, and the ESP32-S3 and display share ground. ESP32-S3 signal pins use 3.3 V logic. Use a suitable backlight driver if you want PWM control from GPIO 14.

[Full wiring diagram and optional GPIOs](PIN_MAPPING.md) · [Hardware and power details](HARDWARE.md)

## 2. Build and upload

### Get the complete sketch

Clone into a folder named `cuteOS` so Arduino can match the folder with `cuteOS.ino`. These commands work in PowerShell, Bash, and Zsh with Git installed.

```sh
git clone https://github.com/dsarthikreddy/esp32-s3-touchscreen-ui.git cuteOS
cd cuteOS
```

You can also use **Code → Download ZIP**, extract it, and rename the extracted folder to `cuteOS`. Keep both [cuteOS.ino](cuteOS.ino) and [secrets.example.h](secrets.example.h) in that folder. For manual copying, open each file on GitHub and use **Raw** to get the complete text.

### Arduino IDE

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software). In **File → Preferences → Additional Boards Manager URLs**, paste:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

2. In **Boards Manager**, install **esp32 by Espressif Systems 2.0.17**. In **Library Manager**, install **LovyanGFX by lovyan03 1.3.0**.
3. Open `cuteOS.ino` and set these options under **Tools**.

   | Setting | Value |
   |---|---|
   | Board | ESP32S3 Dev Module |
   | Flash Size | 16MB (128Mb) |
   | PSRAM | **OPI PSRAM** |
   | Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
   | Flash Mode | QIO 80MHz |
   | CPU Frequency | 240MHz (WiFi) |

4. Select the board's port and click **Verify**, then **Upload**. Follow the touch targets on the first boot to save calibration.

<details>
<summary>Build from the command line with Arduino CLI</summary>

Run from the `cuteOS` sketch folder with `arduino-cli` on your PATH.

On Linux, the ESP32 upload tool requires Python 3 and PySerial. If the build reports `No module named 'serial'`, install PySerial for the Python interpreter used by Arduino CLI.

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@2.0.17 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install LovyanGFX@1.3.0
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio .
arduino-cli board list
```

Upload to the port shown by `board list`. Replace `COM3` with that port, such as `/dev/ttyUSB0` on Linux.

```sh
arduino-cli upload --port COM3 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio .
```

</details>

### Enable live content

Copy [secrets.example.h](secrets.example.h) to `secrets.h` beside the sketch. Set your **2.4 GHz Wi-Fi** network name and password, latitude and longitude in decimal degrees, location labels, and UTC offset in seconds. Upload again to apply the settings. Git ignores `secrets.h`.

The example configuration is enough to compile and try the local interface. Live news, maps, weather, and time sync need Wi-Fi.

## Explore the interface

| From Home | Opens |
|---|---|
| Swipe right | Live news |
| Swipe left | Device tiles and notifications |
| Swipe down | Control Center and brightness |
| Swipe up | App drawer with RF radar, oscilloscope, RGB/GPIO controls, calculator, and focus timer |
| Tap the top pill | Wi-Fi, weather or timer status, and shortcuts |
| Tap **+ Widgets** | Choose the two Home widgets |

## Troubleshooting

| Symptom | Check first |
|---|---|
| Upload port is missing | Use a USB data cable and check the correct connector for your board. |
| White screen or no image | Check display power and backlight, then trace `CS`, `RESET`, `DC`, `SDI`, and `SCK`. |
| Image works but touch fails | Trace all four touch wires: `T_CLK`, `T_CS`, `T_DIN`, and `T_DO`. Then rerun calibration from Control Center. |
| **ENABLE OPI PSRAM!** | Select **OPI PSRAM** and confirm the board is the N16R8 variant. |
| `LovyanGFX.hpp` missing | Install **LovyanGFX 1.3.0** in the IDE or CLI environment compiling the sketch. |
| Live widgets show fallback content | Check `secrets.h`, 2.4 GHz Wi-Fi, and internet access. |

## Project reference

Owner-reported hardware testing covers display, touch, networking, and peripherals. See the [test matrix](TEST_RESULTS.md) for the full scope.

| Looking for | Read |
|---|---|
| Code and configuration | [Firmware](cuteOS.ino) · [Configuration template](secrets.example.h) · [Dependencies](DEPENDENCIES.md) |
| Hardware and display | [Pin map](PIN_MAPPING.md) · [Hardware](HARDWARE.md) · [Display and touch](DISPLAY_TOUCH.md) |
| Design and architecture | [UI design](UI_DESIGN.md) · [Architecture](ARCHITECTURE.md) · [Design decisions](DECISIONS.md) |
| Maintenance | [Known issues](BUGS.md) · [Roadmap](TASKS.md) · [Changelog](CHANGELOG.md) · [Handoff](HANDOFF.md) |
