# PlatformIO_esp32-4ch-thyristor-board-illumination

PlatformIO firmware for a mains-powered ESP32 board that switches four 230 V lighting channels. The power stage is four BT137S triacs driven by MOC3063 zero-cross optotriacs, so each channel is fully on or fully off. It does not dim.

Hardware notes, terminal map, and what the seller documents do and do not say: [docs/HARDWARE.md](docs/HARDWARE.md).

This repository is the firmware submodule used by [ServerBootShutdownManagemement](https://github.com/tinel-c/ServerBootShutdownManagemement) at `device/PlatformIO_esp32-4ch-thyristor-board-illumination`.

## Safety

The board is powered from 230 V AC and the outputs are live mains. Treat every terminal as live once the input is connected.

- First flash and GPIO tracing happen with the mains unplugged.
- `BOARD_PINS_CONFIRMED` in [include/board_pins.h](include/board_pins.h) ships as `0`. MQTT commands are remembered and reported, but no GPIO is driven, so a wrong pin cannot turn a channel on.
- Set the four channel GPIOs only after each MOC3063 input is traced to a header pin. Then set `BOARD_PINS_CONFIRMED` to `1`.
- Boot, every reboot, and every OTA start force all four channels off. Command messages are not retained, and the firmware does not restore a previous state.
- Confirm the Serial Burn header voltage on the silkscreen before tying a USB-UART adapter's VCC into that header. The ESP32-WROOM-32E module supply is 3.3 V. A pin marked 5 V is only safe if it feeds the onboard regulator, not the module.

## MQTT

Broker default is the automation server: `192.168.2.4:1883`, anonymous (Mosquitto 2.0.18).

| Item | Value |
|------|--------|
| Client id | `PlatformIO_esp32-4ch-thyristor-board-illumination` |
| Status | `illumination/4ch-thyristor/status` (QoS 1, not retained) |
| Command | `illumination/4ch-thyristor/command/set` (QoS 1, not retained) |
| OTA status | `illumination/4ch-thyristor/ota/status` |
| OTA progress | `illumination/4ch-thyristor/ota/progress` |
| DHCP / ArduinoOTA hostname | `esp32-4ch-thyristor` |
| OTA port | 3232 |

Status, every 30 s and after each command:

```json
{
  "device": "PlatformIO_esp32-4ch-thyristor-board-illumination",
  "fw": "0.2.0",
  "outputs_enabled": false,
  "ip": "192.168.2.61",
  "ota_port": 3232,
  "channels": [
    {"id": 1, "state": "off"},
    {"id": 2, "state": "off"},
    {"id": 3, "state": "off"},
    {"id": 4, "state": "off"}
  ],
  "uptime_s": 12,
  "rssi": -61
}
```

`ip` and `ota_port` are required by the monorepo fleet tool for automatic OTA. `outputs_enabled` is false until the pin map is confirmed. `state` is the commanded state either way.

Commands:

```json
{"channel": 1, "state": "on"}
{"channel": 4, "state": "off"}
{"channel": 0, "state": "off"}
```

`channel` 0 with `state` `off` turns every channel off. Any other `channel` 0 payload is ignored.

## Build and flash

Requirements: [PlatformIO](https://platformio.org/) (CLI or the IDE extension).

### First flash (UART)

There is no USB connector. Use the **Serial Burn** header (GND, IO0, RXD, TXD, and a power pin). Hold BOOT (IO0), tap Reset (EN), then release BOOT. Cross RX/TX with the adapter.

```bash
cp include/secrets.h.example include/secrets.h
# edit Wi-Fi credentials in include/secrets.h
pio run -e esp32dev -t upload --upload-port COMx
pio device monitor -b 115200
```

(`upload_protocol = espota` is set for later updates; for the first wired flash pass `--upload-protocol esptool` if your PlatformIO build of the project requires it.)

### Later updates (automatic OTA)

When this repo is a submodule of ServerBootShutdownManagemement, a successful `pio run -e esp32dev` builds `firmware.bin` and then runs [scripts/ota_after_build.py](scripts/ota_after_build.py). That script calls:

```bash
python scripts/status/device_fleet.py flash \
  --id PlatformIO_esp32-4ch-thyristor-board-illumination \
  --bin .pio/build/esp32dev/firmware.bin
```

in the monorepo. The fleet tool refreshes MQTT sightings into `scripts/status/device_fleet_state.json` and runs `espota.py` when the board published a fresh `ip` within the timeout. Offline or missing IP leaves the `.bin` and still exits successfully. A failed OTA to a live board fails the post-build step.

Keep the sighting warm while developing:

```bash
# from the monorepo root
python scripts/status/device_fleet.py watch
```

An optional OTA password can be set via `OTA_PASSWORD` in `include/secrets.h` (and passed to the fleet tool with `--password` or `OTA_PASSWORD` in the environment).

## Layout

| Path | Role |
|------|------|
| `src/main.cpp` | Wi-Fi, MQTT, OTA, channel commands |
| `include/board_pins.h` | Channel GPIOs and the confirmed-pin gate |
| `include/secrets.h.example` | Wi-Fi and broker template |
| `scripts/ota_after_build.py` | Post-build hook → monorepo fleet flash |
| `docs/HARDWARE.md` | Board specification |
