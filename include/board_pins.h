#pragma once

// Confirmed on the LC Tech / YOSUN board (seller silk: RY1–RY4).
#define BOARD_PINS_CONFIRMED 1

// RY1 IO32, RY2 IO25, RY3 IO27, RY4 IO12
#define PIN_CH1 32
#define PIN_CH2 25
#define PIN_CH3 27
#define PIN_CH4 12

// Blue LED. Wi-Fi indicator, not a channel. See updateWifiLed().
#define PIN_STATUS_LED 5

// MOC3063 LED on when the GPIO is high. L1/L3/L4 were confirmed active-high.
#define CHANNEL_ACTIVE_LEVEL HIGH
#define CHANNEL_IDLE_LEVEL LOW

// Blue LED lights when IO5 is low.
#define LED_ACTIVE_LEVEL LOW
#define LED_IDLE_LEVEL HIGH
