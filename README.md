<p align="center">
  <a href="https://www.espressif.com/en/products/hardware/esp32/resources">
    <img alt="Espressif" src="https://www.espressif.com/sites/all/themes/espressif/logo.svg" width="500" />
  </a>
  <a href="https://platformio.org/">
    <img alt="PlatformIO" src="https://cdn.platformio.org/images/platformio-logo.17fdc3bc.png" width="100" />
  </a>
</p>
<h1 align="center">
  ESP32 Victron MQTT Sender for PlatformIO
</h1>

Contains a PlatformIO project for Espressif ESP32 that will discover and connect to an MQTT broker and publish data from up to three Victron ve.direct devices in real time.

## ⚙️ Gear you need

### 🛠 Hardware

You need an ESP32 development board. I used a [NodeMCU-32s](https://www.amazon.com/dp/B07PP1R8YK/ref=twister_B0816C3VDG?_encoding=UTF8&psc=1). Also a few generic NPN transistors to shift the level of the (potential) 5V RS-232 signaling from the Victron unit to the 3.3V needed by the ESP32. An optoisolator providing galvanic isolation would be preferable, but I didn't have any on hand.

### 📀 Software

You need a [PlatformIO](https://platformio.org/) development environment, of course. It must be set up for ESP32 development, including the sketch data uploader. I'm running PlatformIO from VSCode and it's been a great fit so far.

You will also need the ArduinoJSON and AsyncMqttClient-esphome libraries, as well as the Adafruit Si7021 Library if you want temperature and humidity. They are referenced from `platformio.ini`, so hopefully the platform will just pull them in for you. I'm still a bit new to PlatformIO so I'm not sure how it handles this.

## 🧩 Getting set up

Once you have this project downloaded there are still a couple of things to do.

### 👫 Hookups

See the file `Schematic_esp32-ve.direct-mqtt.pdf` for how to hook up the ve.direct TX & ground lines to your ESP32. The ve.direct ground & TX lines are on pin 1 & 3 of the specified JST connector, see the file `VE.Direct-Protocol-3.29.pdf` for details.

### 📡 WiFi credentials

You will need to rename the file `sample.config.json` to `config.json` and move it to the `data` directory. Edit the file to reflect the ssid and key for your network. The ESP32 will connect to this network and attempt to establish an mDNS responder. The name of the mDNS responder is also specified in `config.json` and can be changed to your liking.

## 🚀 Launching the project

First you will need to build and launch the MQTT discovery agent (code coming soon). You will need to point it at the MQTT broker you wish the project to report its data to.

Upload sketch data to the board first, then upload the sketch. If you haven't changed the name of the mDNS responder in `config.json` then your board will now be available at `victron-mqtt.local`.

Windows users: Windows 10 (and possibly earlier versions) does not do mDNS by default, meaning that the '.local' addresses will not work. Ironically, downloading and installing the [Apple BonJour print services](https://support.apple.com/kb/dl999?locale=en_US) enables mDNS.

<p align="center" style="padding-top: 50">🍀 Good Luck! 🍀
