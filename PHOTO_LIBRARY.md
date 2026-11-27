# Photo library

cuteOS opens up to 24 JPEGs from `/photos` in LittleFS. Resize photos to fit within **424 × 206 pixels** before adding them. Use standard baseline JPEG to keep decoding fast. Open **Photos** from the app drawer and swipe or use the arrows to browse.

## Load photos into flash

These values apply to the documented ESP32 core **2.0.17** and **Huge APP** partition scheme. Loading the image replaces the contents of its filesystem partition. Use a board layout with LittleFS at `0x310000` and size `0xE0000` before running the commands below.

1. In a temporary folder, create `data/photos` and add your JPEGs.
2. Use the `mklittlefs` tool bundled with the ESP32 core to create the filesystem image.
3. Put the board in upload mode and write the image to the filesystem offset. Replace `COM3` with your serial port.

```sh
mklittlefs -c data -b 4096 -p 256 -s 917504 photos.bin
python -m esptool --chip esp32s3 --port COM3 write_flash 0x310000 photos.bin
```

After the board restarts, open **Photos**. Tap **Reload** to rescan the image list.
