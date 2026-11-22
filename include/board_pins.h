#pragma once

// Confirmed on the LC Tech / YOSUN board (seller silk: RY1–RY4).
#define BOARD_PINS_CONFIRMED 1

// RY1 IO32, RY2 IO26, RY3 IO27, RY4 IO12
#define PIN_CH1 32
#define PIN_CH2 26
#define PIN_CH3 27
#define PIN_CH4 12

// Working-status LED (seller silk / board wiring).
#define PIN_STATUS_LED 5

// MOC3063 LED on when the GPIO is high. Invert here if a channel turns on when the pin is low.
#define CHANNEL_ACTIVE_LEVEL HIGH
#define CHANNEL_IDLE_LEVEL LOW
