# Hardware — ESP32 4-channel BT137S thyristor switch

Illumination controller for 230 V AC. Four independent on/off channels. Not a dimmer.

## Identity

| Item | Value |
|------|--------|
| Role in this project | `PlatformIO_esp32-4ch-thyristor-board-illumination` |
| AliExpress item | [1005011922415917](https://www.aliexpress.com/item/1005011922415917.html) |
| Listing title | ESP32 4 channel BT137S thyristor switch output module development board AC powered |
| Seller | YOSUN Store (listing brand field: EYEWINK) |
| Manufacturer page | [LC Tech / 深圳市艾尔赛, product 906](http://www.lctech-inc.com/cpzx/2/jdqmk/2026/0123/906.html) — ESP32四路 BT137S可控硅开关输出模块 |
| PCB silkscreen | `5437314L1_V20_251004` (rev V2.0, date 2025-10-04) |
| MCU module | ESP32-WROOM-32E (`ESP32-32E` on the shield). Wi-Fi 802.11 b/g/n and Bluetooth. No USB |

The AliExpress “User manual (PDF)” is a generic EU template. It lists 3–5 V, a battery, and unrelated instructions. Do not use it as a specification. The compliance block on that PDF also names a different company than the LC Tech product page. The board identity used here is the PCB photo and the LC Tech title, not that PDF.

Listing attributes that are filler and not used: package “SMD”, size “5 × 5 × 5 cm”, weight 0.120 kg. A reseller listing of the same module quotes the PCB at about 87 × 93 mm, which matches the square board in the photos. Measure the board in hand before designing an enclosure.

## What the photos show

Seller description image (annotated top view) and the bench photo at 230 V:

- **AC input:** one 2-pole screw terminal, silkscreen **L** and **N**. The board’s own AC-DC supply (yellow transformer, electrolytic capacitors, varistor, blue safety capacitor) runs from this input. There is no separate DC jack.
- **Outputs:** screw terminals marked **L1 N**, **L2 N**, **L3 N**, **L4 N**. Each channel is a switched live with a neutral terminal beside it.
- **Power devices:** four DPAK triacs marked **BT137S**, on the mains side of a milled isolation slot.
- **Drivers:** four **MOC3063** optotriacs on the logic side of that slot. MOC3063 is a zero-cross triac driver. While its LED is on, the BT137S is retriggered at each mains zero crossing and the load sees full line voltage. Releasing the LED lets the triac commutate off at the next current zero. There is no phase-angle input and no exposed zero-cross pin, so this board cannot dim.
- **Factory demo (seller text, not this firmware):** the IO0 button cycles five modes. Power-up is all channels off. Each press enables one more channel, up to all four on, then wraps back to all off. The seller’s meter shot shows about 230.9 V, 50 Hz, on an output in an “on” mode. A status LED tracks the mode (off, then up to fully lit).
- **ESP32:** module soldered on the logic side. Two breakout headers carry spare GPIOs (IO0, IO2, IO4, IO5, IO12–IO19, IO21–IO23 and the high GPIOs are visible; read the silkscreen on the real board). Those headers are not a labelled map of the four triac gates.
- **Serial Burn header:** six pads. The photo reads as GND, GND, IO0, RXD, TXD, and a supply pin that looks like **5V**. Confirm that last label on the physical board before connecting an adapter. ESP32-WROOM-32E VDD is 3.0–3.6 V. If the pad is 5 V it must be the regulator input, not the module pin.
- **Buttons:** BOOT (IO0) and Reset (EN).
- **LEDs:** power indicator (likely the 3.3 V rail) and a blue Wi-Fi indicator on **IO5**.
- **Protection visible on the mains side:** varistor across the input, safety capacitor, and the isolation slot. The BT137S tabs are the triac main terminal (T2) and are live. Do not probe them with a grounded instrument while the board is powered.

## Electrical limits

BT137S (WeEn / NXP family, SOT428) is an 8 A RMS, 600 V four-quadrant triac. That is the **die** rating at a case temperature the bare PCB cannot hold. This module has no heatsink, small screw terminals, and no published board current. Use it as a lighting switch. Confirm the terminal and copper limit on the physical board before connecting a load, and keep the first live test to one low-power resistive lamp.

MOC3063 output is a zero-cross triac (typical 600 V, low trigger current). It is appropriate for resistive lamps and for LED lamps that tolerate a zero-cross solid-state switch. It is a poor match for phase-cut dimmable lamps and for heavily inductive loads. The seller’s own test procedure is full-on voltage, not a dimming curve.

Mains here is 230 V, 50 Hz. The listing says AC 220 V. Both are the same class of supply for this board.

## Power for Wi-Fi and OTA

The board’s AC-DC supply from **L/N** is the normal run path. Flashing or monitoring from Serial Burn alone (USB-UART 5 V) often trips the ESP32 brownout detector when Wi-Fi TX starts. A brownout loop never publishes MQTT `ip`, so fleet OTA cannot run. Use mains for soak tests; for UART flash, GND/RX/TX (+ BOOT/Reset) is enough if mains already powers the board — do not back-feed 5 V from the adapter onto a live board.

## OTA and fleet memory

ArduinoOTA hostname **`esp32-4ch-thyristor`**, UDP/TCP port **3232**. Firmware ≥ **0.2.2** calls `ArduinoOTA.begin()` only after STA has an IP; earlier builds bound too early and never answered invitations. Idle OTA is UDP (a TCP connect probe to 3232 may show closed). Status MQTT includes `ip` and `ota_port` for [scripts/status/device_fleet.py](../../../scripts/status/device_fleet.py). OTA start and reboot force every channel off.

## GPIO map

Confirmed channel gates (seller silk **RY1–RY4** → ESP32 GPIO), set in [include/board_pins.h](../include/board_pins.h) with `BOARD_PINS_CONFIRMED 1`:

| Channel | Silk | GPIO |
|---------|------|------|
| 1 | RY1 | IO32 |
| 2 | RY2 | IO25 |
| 3 | RY3 | IO27 |
| 4 | RY4 | IO12 |
| Status LED | — | IO5 |

GPIO12 is an ESP32 strapping pin (MTDI). Leave it low at reset so flash voltage mode stays correct; the firmware holds every channel idle (low) from boot and drives IO12 low again before an OTA reboot. IO0 stays the boot strap; this firmware does not reuse the factory button demo.

Active level is GPIO high = channel on. Boot and OTA start still force every channel off.

The blue LED on IO5 lights when the pin is low. It shows Wi-Fi only. The patterns are the fleet standard in [include/wifi_status_led.h](../include/wifi_status_led.h) (template: `device/templates/platformio-esp32/include/wifi_status_led.h`):

| Pattern | Meaning |
|---------|---------|
| On, short wink every 2 s | Associated |
| Fast blink (100 ms) for 3 s after each join | Joining the AP |
| 500 ms blink | Not associated, waiting to retry |
| Two short flashes | No SSID, connect failed, or link lost |

## References

- Listing: https://www.aliexpress.com/item/1005011922415917.html
- LC Tech product page: http://www.lctech-inc.com/cpzx/2/jdqmk/2026/0123/906.html
- Seller description (modes and 220 V output test): `pdp.aliexpress-media.com` product description for item 1005011922415917
- BT137S family datasheet (8 A, 600 V, DPAK): WeEn BT137S-600E
- MOC3063: zero-cross optotriac, 600 V class (Lite-On / Onsemi family)
