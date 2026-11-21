# PlatformIO_esp32-4ch-thyristor-board-illumination

PlatformIO firmware for a mains-powered ESP32 board that switches four 230 V lighting channels. The power stage is four BT137S triacs driven by MOC3063 zero-cross optotriacs, so each channel is fully on or fully off. It does not dim.

Hardware notes, terminal map, and what the seller documents do and do not say: [docs/HARDWARE.md](docs/HARDWARE.md).

This repository is the firmware submodule used by [ServerBootShutdownManagemement](https://github.com/tinel-c/ServerBootShutdownManagemement) at `device/PlatformIO_esp32-4ch-thyristor-board-illumination`.

## Safety

The board is powered from 230 V AC and the outputs are live mains. Treat every terminal as live once the input is connected.

- First flash and GPIO tracing happen with the mains unplugged.
- `BOARD_PINS_CONFIRMED` in [include/board_pins.h](include/board_pins.h) ships as `0`. MQTT commands are remembered and reported, but no GPIO is driven, so a wrong pin cannot turn a channel on.
- Set the four channel GPIOs only after each MOC3063 input is traced to a header pin. Then set `BOARD_PINS_CONFIRMED` to `1`.
- Boot and every reboot start with all four channels off. Command messages are not retained, and the firmware does not restore a previous state.
- Confirm the Serial Burn header voltage on the silkscreen before tying a USB-UART adapter's VCC into that header. The ESP32-WROOM-32E module supply is 3.3 V. A pin marked 5 V is only safe if it feeds the onboard regulator, not the module.

## MQTT

Broker default is the automation server: `192.168.2.4:1883`, anonymous (Mosquitto 2.0.18).

| Item | Value |
|------|--------|
| Client id | `PlatformIO_esp32-4ch-thyristor-board-illumination` |
| Status | `illumination/4ch-thyristor/status` (QoS 1, not retained) |
| Command | `illumination/4ch-thyristor/command/set` (QoS 1, not retained) |
| OTA hostname | same as the client id |
| DHCP hostname | `esp32-4ch-thyristor` (the full client id is longer than the ESP32 Wi-Fi hostname limit) |

Status, every 30 s and after each command:

```json
{
  "device": "PlatformIO_esp32-4ch-thyristor-board-illumination",
  "fw": "0.1.0",
  "outputs_enabled": false,
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

`outputs_enabled` is false until the pin map is confirmed. `state` is the commanded state either way.

Commands:

```json
{"channel": 1, "state": "on"}
{"channel": 4, "state": "off"}
{"channel": 0, "state": "off"}
```

`channel` 0 with `state` `off` turns every channel off. Any other `channel` 0 payload is ignored.

## Build and flash

Requirements: [PlatformIO](https://platformio.org/) (CLI or the IDE extension).

```bash
cp include/secrets.h.example include/secrets.h
# edit Wi-Fi credentials in include/secrets.h
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

There is no USB connector. Use the **Serial Burn** header (GND, IO0, RXD, TXD, and a power pin). Hold BOOT (IO0), tap Reset (EN), then release BOOT so the ROM console starts. Cross RX/TX with the adapter. Do not connect the adapter VCC until the silkscreen voltage is known; GND, RX, TX, and IO0 are enough when the board is already powered from a confirmed low-voltage source.

After the device is on the LAN, ArduinoOTA is available under the client-id hostname. An OTA password is optional via `OTA_PASSWORD` in `include/secrets.h`.

## Layout

| Path | Role |
|------|------|
| `src/main.cpp` | Wi-Fi, MQTT, OTA, channel commands |
| `include/board_pins.h` | Channel GPIOs and the confirmed-pin gate |
| `include/secrets.h.example` | Wi-Fi and broker template |
| `docs/HARDWARE.md` | Board specification |
