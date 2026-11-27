<div align="center">

# cuteOS

**A glass-inspired touchscreen interface for the ESP32-S3**

480 × 320 landscape · ILI9488 display · XPT2046 resistive touch · Arduino + LovyanGFX

[Download cuteOS](https://github.com/dsarthikreddy/esp32-s3-touchscreen-ui/releases/latest) · [Wire the display](#1-wire-the-display-and-touch) · [Build and upload](#2-build-and-upload) · [Pair Android](companion/README.md) · [Use the display](#explore-the-interface)

</div>

---

## Start here

1. Download **`cuteOS.zip`** and extract it. It contains a ready-to-open **`cuteOS`** folder with the Arduino sketch, headers, and Android project together. [Latest release](https://github.com/dsarthikreddy/esp32-s3-touchscreen-ui/releases/latest)
2. With the board wired, open **`cuteOS/cuteOS.ino`** in Arduino IDE and upload using the [board settings below](#arduino-ide).
3. For phone features, open **`cuteOS/companion`** in Android Studio. Run it on your Pixel and pair it with the display. [Android setup](companion/README.md)

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

On Windows, right-click the downloaded **`cuteOS.zip`**, choose **Extract All**, and open the **`cuteOS`** folder inside. Double-click `cuteOS.ino` from that folder. Arduino uses the matching folder name to find `PhoneLink.h`, the other headers, and `secrets.example.h`.

If you use GitHub's **Code → Download ZIP** instead, rename the extracted folder containing `cuteOS.ino` to **`cuteOS`** before opening the sketch. If you use Git, clone the repository into the correct folder name.

```sh
git clone https://github.com/dsarthikreddy/esp32-s3-touchscreen-ui.git cuteOS
cd cuteOS
```

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

4. Plug the ESP32 into your computer with a USB data cable. Select its **COM** port under **Tools → Port**. Click the **→ Upload** arrow at the top left. Wait for **Done uploading**, then follow the touch targets if the board asks for calibration.

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

Copy [secrets.example.h](secrets.example.h) to `secrets.h` beside the sketch. Set your **2.4 GHz Wi-Fi** network name and password, latitude and longitude in decimal degrees, location labels, and UTC offset in seconds. For Hyderabad, set `SECRET_NTP_OFFSET_SEC` to `19800`. Upload again to apply the settings. Git ignores `secrets.h`.

The example configuration is enough to compile and try the local interface. Live news, maps, weather, and time sync need Wi-Fi.

## Explore the interface

| From Home | Opens |
|---|---|
| Swipe right | News. Keep swiping right for Phone notifications, battery, and Markets. |
| Swipe left | Now Playing. Keep swiping left for device tiles, then World Clock. |
| Swipe down | Control Center and brightness |
| Swipe up | App drawer with Map, Calendar, Photos, Phone, and device tools |
| Tap the top pill | Wi-Fi, weather or timer status, and shortcuts |
| Tap **+ Widgets** | Choose the two Home widgets |

### Controls, phone, and apps

- **Control Center** opens with a swipe down from Home. Tap **Resize**, choose a tile, and drag its marked bottom corner sideways or up and down. Tap **Done** to save the size. Each tile fits within its own space.
- **Phone and music** use the [Android companion app](companion/README.md) for pairing, phone notifications, battery, and media controls. Music shows artwork when the active Android player shares it.
- **Wi-Fi Nearby** shows nearby 2.4 GHz networks by channel. Open it from the device tiles and tap the chart to scan. Taller bars indicate stronger signals.
- **Startup** displays Home before it connects to Wi-Fi. Weather and wallpaper load in the background, and news starts loading when opened. To measure a normal boot, open Arduino IDE's Serial Monitor at **115200 baud** and reset the board. The **Home drawn** and **Touch loop ready** lines report elapsed time. A first boot that needs touch calibration includes time for the user to tap the targets.
- **Map** opens at the configured location. Tap **Load Map** to retrieve tiles, then **Hyderabad** to jump to the city. Drag at zoom level 17. Nearby tiles are cached in PSRAM.
- **Calendar** shows the synced date and time. Single arrows change month and double arrows change year through 2099. **World Clock** uses the same internet time source for ten cities.
- **Photos** reads JPEGs from `/photos` in LittleFS. It decodes the selected image directly from flash and keeps only the file list. See [photo setup](PHOTO_LIBRARY.md).

## Troubleshooting

| Symptom | Check first |
|---|---|
| Upload port is missing | Use a USB data cable and check the correct connector for your board. |
| White screen or no image | Check display power and backlight, then trace `CS`, `RESET`, `DC`, `SDI`, and `SCK`. |
| Image works but touch fails | Trace all four touch wires: `T_CLK`, `T_CS`, `T_DIN`, and `T_DO`. Then rerun calibration from Control Center. |
| **ENABLE OPI PSRAM!** | Select **OPI PSRAM** and confirm the board is the N16R8 variant. |
| `PhoneLink.h: No such file or directory` | Reopen `cuteOS.ino` from the full extracted `cuteOS` folder. Check that `PhoneLink.h`, `PhonePanels.h`, `AtlasPanels.h`, `MarketPanels.h`, and `secrets.example.h` are beside it. |
| `LovyanGFX.hpp` missing | Install **LovyanGFX 1.3.0** in the IDE or CLI environment compiling the sketch. |
| Live widgets show fallback content | Check `secrets.h`, 2.4 GHz Wi-Fi, and internet access. |

## Project reference

The owner tested the full Glass Update on their ESP32-S3 touchscreen and Pixel 9 and confirmed that all features worked on that setup. See the [test results](TEST_RESULTS.md) for details.

| Looking for | Read |
|---|---|
| Code and configuration | [Firmware](cuteOS.ino) · [Configuration template](secrets.example.h) · [Dependencies](DEPENDENCIES.md) |
| Hardware and display | [Pin map](PIN_MAPPING.md) · [Hardware](HARDWARE.md) · [Display and touch](DISPLAY_TOUCH.md) |
| Design and architecture | [UI design](UI_DESIGN.md) · [Architecture](ARCHITECTURE.md) · [Design decisions](DECISIONS.md) |
| Maintenance | [Known issues](BUGS.md) · [Roadmap](TASKS.md) · [Changelog](CHANGELOG.md) · [Handoff](HANDOFF.md) |
