# Install ARose WaveShare

[Back to README](../README.md) · [Český návod](FLASHING.cs.md)

Use a **Waveshare ESP32-S3-Touch-LCD-2.1** or **ESP32-S3-Touch-LCD-2.8C** with **16 MB flash and 8 MB OPI PSRAM**. One image detects both models. Current XIP firmware has been tested on 2.1 hardware; final XIP/settings verification on 2.8C remains open.

## Browser installation

1. Download **`ARose-WaveShare-0.3.0.factory.bin`** from [release v0.3.0](https://github.com/CooLajz/ARose-WaveShare/releases/tag/v0.3.0).
2. Connect the board using a USB data cable. Close serial monitors that have the port open.
3. Open the official [Espressif web flasher](https://espressif.github.io/esptool-js/) in desktop Chrome or Edge. Connect to the board's USB serial port.
4. Select the factory `.bin` file at flash address **`0x0`**. Select **DIO**, **80 MHz**, **16 MB** if the flasher asks for those parameters. A baud rate of 460800 is a useful starting point.
5. Leave **Erase all** unchecked unless you intentionally want to remove all previous flash data. Program the image, then reset or reconnect power to the board.

The animation starts automatically. Hold a finger on the display for about 0.7 seconds to open settings. For a clean configuration, backlight starts at **100%**, point luminosity at **50%**, and glow at **80%**. Saving position is enabled with a two-minute interval. Compatible existing ARose settings take precedence over defaults.

If the serial connection does not enter the bootloader automatically, hold **BOOT**, tap **RESET**, then release **BOOT** and reconnect. Use the controls available on your particular board. If communication is unreliable, try 115200 baud and another data cable.

## What is in the file?

The factory file is a merged image containing:

| Offset | Component |
| --- | --- |
| `0x0000` | ESP32-S3 bootloader, DIO / 80 MHz / 16 MB header |
| `0x8000` | Partition table |
| `0xe000` | Initial OTA slot selection |
| `0x10000` | ARose XIP application |

**Flash the factory file at `0x0`, not at `0x10000`.** It is not an OTA image and must not be sent through the clocks' OTA updater.

The merged file ends shortly after the application; it is not padded to all 16 MB. Gaps inside a raw merged image are filled with `0xff`, including the ordinary NVS area at `0x9000`. The separate `clockcfg` partition at `0xfe0000`, which contains ARose settings and the clocks' settings namespaces, lies outside this file. A full-chip erase performed by a flasher would remove it too.

For component-by-component installation, `ARose-WaveShare-0.3.0.parts.zip` contains the four files and their offsets. `SHA256SUMS.txt` allows downloaded assets to be checked. The `manifest.json` asset describes the factory image for ESP Web Tools integrations; hosting it requires HTTPS and suitable CORS headers. It does not configure Wi-Fi.

The bootloader and application have been checked with esptool, and package contents are verified against the compiled files. The merged package has not been flashed through a browser in this project session; device tests used the same components through esptool.

## Command-line alternative

With esptool 4.x installed:

```sh
python3 -m esptool --chip esp32s3 --port YOUR_PORT --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 ARose-WaveShare-0.3.0.factory.bin
```

The demo does not use Wi-Fi, a network service or an OTA endpoint. To change firmware later, flash the desired project again over USB.

References: [Espressif esptool merge/flash documentation](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/esptool/basic-commands.html), [ESP Web Tools firmware preparation](https://esphome.github.io/esp-web-tools/#prepare-your-firmware).
