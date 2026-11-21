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
- **LEDs:** power indicator (likely the 3.3 V rail) and a working-status indicator.
- **Protection visible on the mains side:** varistor across the input, safety capacitor, and the isolation slot. The BT137S tabs are the triac main terminal (T2) and are live. Do not probe them with a grounded instrument while the board is powered.

## Electrical limits

BT137S (WeEn / NXP family, SOT428) is an 8 A RMS, 600 V four-quadrant triac. That is the **die** rating at a case temperature the bare PCB cannot hold. This module has no heatsink, small screw terminals, and no published board current. Use it as a lighting switch. Confirm the terminal and copper limit on the physical board before connecting a load, and keep the first live test to one low-power resistive lamp.

MOC3063 output is a zero-cross triac (typical 600 V, low trigger current). It is appropriate for resistive lamps and for LED lamps that tolerate a zero-cross solid-state switch. It is a poor match for phase-cut dimmable lamps and for heavily inductive loads. The seller’s own test procedure is full-on voltage, not a dimming curve.

Mains here is 230 V, 50 Hz. The listing says AC 220 V. Both are the same class of supply for this board.

## OTA and fleet memory

ArduinoOTA listens on port **3232** with hostname **`esp32-4ch-thyristor`**. Status MQTT includes `ip` and `ota_port` so the monorepo tool [scripts/status/device_fleet.py](../../../scripts/status/device_fleet.py) can remember the board and flash after a PlatformIO compile. OTA start forces every channel off before the firmware image is written. Reboot after OTA also boots with every channel off.

## GPIO map

Not published. The four MOC3063 LEDs are wired to ESP32 GPIOs that are not named on the silkscreen next to the triacs.

Bring-up, mains unplugged:

1. Continuity from each MOC3063 LED anode/cathode resistor to a GPIO on the breakout.
2. Write those four numbers into `include/board_pins.h`.
3. Set `BOARD_PINS_CONFIRMED` to `1` and flash again.
4. Only then connect a lamp and the AC input.

Until step 3, this firmware never calls `pinMode` or `digitalWrite` on a channel pin. IO0 stays the boot strap; this firmware does not reuse the factory button demo.

Active level defaults to GPIO high = channel on, which is the usual MOC3063 LED wiring. If a channel turns on when the pin is low, swap `CHANNEL_ACTIVE_LEVEL` / `CHANNEL_IDLE_LEVEL` in `board_pins.h`. Boot still forces every channel off before Wi-Fi starts.

## References

- Listing: https://www.aliexpress.com/item/1005011922415917.html
- LC Tech product page: http://www.lctech-inc.com/cpzx/2/jdqmk/2026/0123/906.html
- Seller description (modes and 220 V output test): `pdp.aliexpress-media.com` product description for item 1005011922415917
- BT137S family datasheet (8 A, 600 V, DPAK): WeEn BT137S-600E
- MOC3063: zero-cross optotriac, 600 V class (Lite-On / Onsemi family)
