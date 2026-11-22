# PlatformIO_esp32-4ch-thyristor-board-illumination

PlatformIO firmware for a mains-powered ESP32 board that switches four 230 V lighting channels. The power stage is four BT137S triacs driven by MOC3063 zero-cross optotriacs, so each channel is fully on or fully off. It does not dim.

Hardware notes, terminal map, and what the seller documents do and do not say: [docs/HARDWARE.md](docs/HARDWARE.md).

This repository is the firmware submodule used by [ServerBootShutdownManagemement](https://github.com/tinel-c/ServerBootShutdownManagemement) at `device/PlatformIO_esp32-4ch-thyristor-board-illumination`.

## Safety

The board is powered from 230 V AC and the outputs are live mains. Treat every terminal as live once the input is connected.

- Channel gates (confirmed): RY1=IO32, RY2=IO26, RY3=IO27, RY4=IO12; status LED=IO5 — see [include/board_pins.h](include/board_pins.h) (`BOARD_PINS_CONFIRMED 1`).
- First live tests: one low-power resistive lamp, AC input last.
- Boot, every reboot, and every OTA start force all four channels off. Command messages are not retained, and the firmware does not restore a previous state.
- Confirm the Serial Burn header voltage on the silkscreen before tying a USB-UART adapter's VCC into that header. The ESP32-WROOM-32E module supply is 3.3 V. A pin marked 5 V is only safe if it feeds the onboard regulator, not the module.
- Prefer **230 V L/N** for normal run. USB-UART 5 V alone often brownouts when Wi-Fi transmits (`Brownout detector was triggered`); that prevents MQTT and OTA.

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
  "fw": "0.2.2",
  "outputs_enabled": true,
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

`ip` and `ota_port` are required by the monorepo fleet tool for automatic OTA. With the confirmed pin map, `outputs_enabled` is true and MQTT commands drive the triac gates. `state` is the commanded channel state.

Commands:

```json
{"channel": 1, "state": "on"}
{"channel": 4, "state": "off"}
{"channel": 0, "state": "off"}
```

`channel` 0 with `state` `off` turns every channel off. Any other `channel` 0 payload is ignored.

## Build and flash

Requirements: [PlatformIO](https://platformio.org/) (CLI or the IDE extension).

### First flash (UART) — required once

Until this firmware is on the board, there is no MQTT `ip` and automatic OTA cannot run (`device offline or sighting stale` is expected). Use the **Serial Burn** header (GND, IO0, RXD, TXD, and a power pin). Hold BOOT (IO0), tap Reset (EN), then release BOOT. Cross RX/TX with the adapter.

```bash
cp include/secrets.h.example include/secrets.h
# edit Wi-Fi credentials in include/secrets.h
# If a live board already has a fleet IP, skip post-build OTA during UART flash:
#   SKIP_FLEET_OTA=1 pio run -e esp32dev -t upload --upload-port COMx
pio run -e esp32dev -t upload --upload-port COMx
pio device monitor -b 115200
```

`upload_protocol` is **esptool** (serial). The IDE Upload button uses the same path — pick the adapter COM port in PlatformIO.

After Wi-Fi connects, serial should show `OTA listening on <ip>:3232`. ArduinoOTA binds only once STA has an IP (fw ≥ 0.2.2); binding earlier left UDP 3232 silent.

### Later updates (automatic OTA after build)

When this repo is a submodule of ServerBootShutdownManagemement, a successful **`pio run -e esp32dev`** (build, not Upload) produces `firmware.bin` and then runs [scripts/ota_after_build.py](scripts/ota_after_build.py). That script calls the monorepo hook `scripts/status/fleet_ota_hook.py`, which runs:

```bash
python scripts/status/device_fleet.py flash \
  --id PlatformIO_esp32-4ch-thyristor-board-illumination \
  --bin .pio/build/esp32dev/firmware.bin
```

in the monorepo. The fleet tool refreshes MQTT sightings into `scripts/status/device_fleet_state.json` and runs `espota.py` when the board published a fresh `ip` within the timeout. Offline or missing IP leaves the `.bin` and still exits **successfully** (do not use PlatformIO Upload for OTA). A failed OTA to a live board fails the post-build step.

On Windows, local `espota` often gets past the UDP invite then fails with `No response from device` because the firewall blocks the ESP’s TCP callback. The fleet tool retries over SSH on `192.168.2.4` (`FLEET_OTA_VIA=auto`; force with `broker` or `local`). Set `SKIP_FLEET_OTA=1` to leave the `.bin` without flashing (useful when UART-flashing a fix).

Keep the sighting warm while developing:

```bash
# from the monorepo root
python scripts/status/device_fleet.py watch
# or one shot:
python scripts/status/device_fleet.py status
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
