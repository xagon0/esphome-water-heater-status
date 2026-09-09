# ESPHome Water Heater Status

[![CI](https://github.com/xagon0/esphome-water-heater-status/actions/workflows/ci.yml/badge.svg)](https://github.com/xagon0/esphome-water-heater-status/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Read a water heater's external green status LED with an ESP32 camera and expose one useful Home Assistant sensor: **Water Heater Status**.

The ESP32 classifies the flashes locally. Home Assistant receives **Running**, **Not running**, **Fault**, or **Unknown** when the interpretation changes. No cloud service, image model, or HA automation is required for detection.

**v0.1.0 is an experimental prerelease.** The included decoder targets the Honeywell/Resideo **WT8840 single-flash / rapid-strobe pattern**. Check your control's manual before using it; a similar-looking valve or another Bradford White ICON generation can use different codes. See [compatibility and validation](docs/compatibility.md).

## What you get

| Status | Interpretation |
|---|---|
| Running | Recognized normal call-for-heat strobe |
| Not running | Recognized normal idle flash |
| Fault | Repeated supported abnormal flash group |
| Unknown | Starting up, classification disabled, unreadable image, missing flashes, or an unsupported pattern |

Home Assistant additionally reports `unavailable` when the ESP32 disconnects. Unknown and unavailable are missing information; do not count them as burner off.

The indicator reports the control's state. This project does not independently verify flame, measure water temperature, or control the heater. Mount the camera over the **external status LED** with a removable light shield, keeping controls and ventilation accessible and the electronics away from hot surfaces. No connection to the gas valve or access inside the combustion chamber is needed.

## Hardware and software

- Freenove ESP32-WROVER-E camera development board, with its camera and PSRAM. This is the tested board; the supplied pinout is **not** for AI-Thinker ESP32-CAM or Freenove ESP32-S3.
- USB power, a data-capable USB cable for the first flash, and a stable mount/light shield. The image should be dark between flashes and predominantly green during a flash.
- 2.4 GHz Wi-Fi and Home Assistant's ESPHome integration.
- ESPHome **2026.8.2**, the version pinned in `requirements.txt` and used by CI. Later versions need validation, particularly the camera/JPEG APIs.

## Install

1. Download the [v0.1.0 source release](https://github.com/xagon0/esphome-water-heater-status/releases/tag/v0.1.0), or clone this repository. Keep `water-heater.yaml`, `blink_decoder.h`, and `led_camera.h` together.
2. Copy `secrets.example.yaml` to `secrets.yaml` and fill in your Wi-Fi details, a unique OTA password, and a new API encryption key. Generate a key locally with `openssl rand -base64 32`. The example key is a placeholder and will not validate.
3. Adjust the substitutions at the top of `water-heater.yaml` if needed. Each device needs a unique `node_name`. The defaults name the HA device **Water Heater** and its primary entity **Status**, normally `sensor.water_heater_status` (HA may add a suffix if that ID is taken).
4. Compile and flash over USB, then add the discovered device through **Settings → Devices & services → ESPHome** using the API key from your own secrets file.
5. Complete [calibration](docs/calibration.md). **On a fresh flash, classification starts off and Status stays Unknown until you enable it.**

### ESPHome Device Builder

Put the YAML and both headers in your Device Builder configuration directory (normally `/config/esphome/`). Merge the four keys from `secrets.example.yaml` into your existing `secrets.yaml`; keep any unrelated entries. Install `water-heater.yaml` through Device Builder. If the board is plugged into a different computer, use its browser-connected USB installation option or download the factory image for that computer.

### Command line

Use Python 3.12 or 3.13 and a C++ toolchain if you also want to run the host tests. ESPHome installs its embedded toolchain during compilation.

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
cp secrets.example.yaml secrets.yaml
# Edit secrets.yaml before continuing.
esphome config water-heater.yaml
esphome run water-heater.yaml
```

`esphome run` prompts for an upload target. For later OTA updates, choose the network target or supply `--device YOUR_DEVICE_IP`. If mDNS cannot cross your VLANs, set `wifi.use_address` to that device's address. No network address is embedded in the shared configuration.

Build your own firmware: the release contains source, not a preconfigured binary. Compiled ESPHome images contain the credentials you supplied.

## Everyday use

Leave **Status** enabled. The camera and three calibration controls are disabled in HA by default. Seven numeric diagnostics stay internal to the ESP32 unless explicitly enabled through a substitution. After calibration, disable any optional entities you enabled to return to one active HA entity.

Local image analysis targets 30 fps; the original mounted board achieved about 27–28 fps. Status is checked locally every 250 ms and published only when it changes. Normal pattern confirmation takes approximately 10–12 seconds. There is no periodic image stream: one preview is sent when HA connects, and additional images require a request. Connection keepalives still occur.

A 60-second stable-state measurement of the original deployment recorded zero repeated state updates and zero images after connection warmup. That is a measured example, not a network bandwidth guarantee.

## Modify and contribute

`water-heater.yaml` contains the board, camera and HA configuration. `led_camera.h` extracts a green signal from a 20×15 overview of a 160×120 JPEG. `blink_decoder.h` contains hardware-independent timing decoders and is the starting point for another control's blink scheme.

Run the host tests with `bash tools/test.sh`. See [CONTRIBUTING.md](CONTRIBUTING.md) for build checks, compatibility reports and the source-only release procedure. Contributions and forks are welcome under the [MIT license](LICENSE).

## Documentation

- [Calibration and troubleshooting](docs/calibration.md)
- [Supported patterns, board pinout and validation limits](docs/compatibility.md)
- [Changelog](CHANGELOG.md)
- [Resideo WT8840/WT8860 manual, Table 4](https://customer.resideo.com/resources/Techlit/TechLitDocuments/34-00000s/34-00013EF.pdf)
- [ESPHome camera documentation](https://esphome.io/components/esp32_camera/)
- [Freenove WROVER kit documentation](https://docs.freenove.com/projects/fnk0060/en/latest/)
