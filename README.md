# ESP32-MiniTV

⚠️ Work in progress ⚠️

This project is a tiny video player designed specifically for the [ESP32-S3-LCD-1.69 from Waveshare](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.69).

It can play MJPEG AVI files from an SD card, or stream video content from a computer over WiFi and Websockets. It features a simple web interface for configuration and can be controlled with a single button.

## Hardware

I sourced parts from Aliexpress and Amazon, and designed the case around them.

### Bill of Materials

One of this project's goals is to keep the BOM as short as possible. Only 3 parts, and 6 easy solder joints are needed. There is no audio amp nor speaker in this project.

| Part | Notes | Link (non affiliated) |
| -- | -- | -- |
| Waveshare ESP32-S3-LCD-1.69 | This is the **non touch version**. The touch version has thicker glass and a different footprint. | [Amazon.fr](https://www.amazon.fr/dp/B0D9PTZ5DY), [Aliexpress](https://fr.aliexpress.com/item/1005009927444668.html) |
| Micro SD card reader breakout board | The case was modeled after this exact board.<br><br>Dimensions: 18x18x20mm | [Amazon.fr](https://www.amazon.fr/dp/B0DRXBF5RW) |
| 400 mAH LiPo battery | **Important:** the default connector is PH2.0 which isn't compatible with the Waveshare dev board. I spliced a [connector](https://www.amazon.fr/dp/B09TDCLZGB) but you can custom order the correct one (JST1.25mm) from the battery seller.<br><br>Technically optional it is possible to power the device via USB C connector.<br><br>Dimensions: 6x25x30mm | [Aliexpress](https://fr.aliexpress.com/item/1005007103616809.html) |
| Micro SD card | FAT32 formatted. Also technically optional since the project can also stream video over local network. | |

### 3D printed case

The case is made of 4 parts: front, back, bezel and button. Glue is required to fix the bezel on the front part, the rest of the assembly snap fits (with pretty tight tolerances) and requires no tools. 

![Assembled parts](stl/cad.png)
![Exploded case](stl/exploded.png)

#### Printing instructions

The case was only tested with Prusa MK4S and PLA. It prints with 0.28mm layers in about 30 minutes and 16 grams of material. I tried my best to limit the need for supports but paint-on supports are recommended in select spots for best results.

| Part | Face on print bed | Paint on support |
| --| --| -- |
| Front | Front face | Inside of USB C slot |
| Back | Vent side | Around the curved edge on the print bed |
| Bezel | Flat side | None |
| Button | Top side | None |

### Assebly instructions

Assembly only takes a few minutes. You'll need a bit of plastic glue or cyanoacrylate glue, a soldering iron, and flush cutter pliers.

#### 1. Solder the header on the SD card breakout board

Solder the 6 pin header to the SD breakout board. Trim the pins underneath.

#### 2. Prepare the SPI cable

The ESP32 dev board comes with a cable that breaks out some IO. We need to cut some of the wires to save room, and use the rest to connect the SD card reader.

Separate the lines that we'll keep from the ones we'll cut according to the table below, triple check everything and cut the unused wires flush against the connector. You can change the suggested wiring if you update the `platformio.ini` file accordingly, but these are the default.

| Keep | Description | Cut |
| -- | -- | -- |
| <span style="color:white;background:black">&nbsp;Black&nbsp;</span> | GPIO18 | |
| <span style="color:white;background:red">&nbsp;Red&nbsp;</span> | GPIO17 | | GPIO17 | |
| | GPIO16 | <span style="color:black;background:green">&nbsp;Green&nbsp;</span> |
| | GPIO3 | <span style="color:black;background:#cea903">&nbsp;Yellow&nbsp;</span> | 
| | GPIO2 | <span style="color:white;background:black">&nbsp;Black&nbsp;</span> |
| <span style="color:white;background:red">&nbsp;Red&nbsp;</span> | SDA (GPIO11) | |
| <span style="color:white;background:#7d3a2a">&nbsp;Brown&nbsp;</span> | SCL (GPIO10) | |
| | U0TXD | <span style="color:black;background:orange">&nbsp;Orange&nbsp;</span> |
| | U0RXD | <span style="color:black;background:green">&nbsp;Green&nbsp;</span> |
| <span style="color:white;background:blue">&nbsp;Blue&nbsp;</span> | 3v3 | |
| <span style="color:white;background:purple">&nbsp;Purple&nbsp;</span> | GND | |
| | 5v | <span style="color:black;background:gray">&nbsp;Gray&nbsp;</span> |

Connect the female pin sockets from the uncut wires to the micro SD board like so:

| Cable color | Label on SD board | ESP32 pin
| -- | -- | -- |
| <span style="color:white;background:blue">&nbsp;Blue&nbsp;</span> | 3v3 | 3v3 |
| <span style="color:white;background:#7d3a2a">&nbsp;Brown&nbsp;</span> | CS | SCL | 
| <span style="color:white;background:red">&nbsp;Red&nbsp;</span> (the one <ins>next to <span style="color:white;background:black">black</span></ins> on the cable) | MOSI | GPIO17 |
| <span style="color:white;background:red">&nbsp;Red&nbsp;</span> (the one <ins>next to <span style="color:white;background:#7d3a2a">brown</span></ins> on the cable) | CLK | SDA |
| <span style="color:white;background:black">&nbsp;Black&nbsp;</span> | MISO | GPIO18 |
| <span style="color:white;background:purple">&nbsp;Purple&nbsp;</span> | GND | GND |

Then wrap a bit of electrical tape around the male pins in order to create a makeshift connector that you can easily unplug and plug back. Detach it from the SD board for now.

**⚠️ Warning:** be very careful never to plug your home made connector the wrong way: you'd swap the Ground and 3v3 lines and and most likely damage the hardware.

#### 3. Assemble the case

*Coming soon.*

## Flashing the firmware

<iframe style="border:none;width:100%;height:400px;" src="https://t0mg.github.io/esp32-minitv/flash.html"></iframe>

This project is built with [PlatformIO](https://platformio.org/). You can use the PlatformIO extension for VSCode or the command line interface.

To build the project locally and flash it to the device, connect the ESP32-S3 board via USB and run:

```bash
platformio run --target upload
```

### Over-the-air updates

Once the initial firmware is flashed, you can perform subsequent updates over the air. 

You can find a copy of the latest OTA firmware [here](https://t0mg.github.io/esp32-minitv/firmware/firmware.bin) or you can build it yourself as explained below.

#### Build the binary

```bash
platformio run
```

Then, navigate to the device's web UI, go to the "Firmware" tab, and upload the `firmware.bin` file located in the `.pio/build/esp32-s3-devkitc-1/` directory of the project.

## Preparing video files

You'll need a FAT32 formatted SD Card, and properly encoded video files (AVI MJPEG). Keep the file names short, and place the files at the root of the SD Card. They will play in alphabetical order.

### Transcoding

You can use [this web page](https://t0mg.github.io/esp32-minitv/transcode.html) to convert video files in the expected format (max. output size 2Gb). It relies on [ffmpeg.wasm](https://github.com/ffmpegwasm/ffmpeg.wasm) for purely local, browser based conversion.

For much faster conversion, the `ffmpeg` command line tool is recommended. 

*TODO: add detailed command line*

## Credits and references
- This project is relying heavily on [esp32-tv by atomic14](https://github.com/atomic14/esp32-tv) and the related [blog](http://www.atomic14.com) and [videos](https://www.youtube.com/atomic14). Many thanks !
- Another great source was [moononournation's MiniTV](https://github.com/moononournation/MiniTV).
- The [Waveshare wiki page](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.69) and provideed examples were extremyly useful.
- The web UI uses the VCR OSD Mono font by Riciery Leal.
- Github pages hosted [transcoder tool](https://t0mg.github.io/esp32-minitv/transcode.html) inspired by [this post](https://dannadori.medium.com/how-to-deploy-ffmpeg-wasm-application-to-github-pages-76d1ca143b17), uses [coi-serviceworker](https://github.com/gzuidhof/coi-serviceworker) to load [ffmpeg.wasm](https://github.com/ffmpegwasm/ffmpeg.wasm).
