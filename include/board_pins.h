#pragma once

// Channel gate GPIOs are not printed on the board. Leave this at 0 until each
// MOC3063 input has been traced to a header pin with the mains disconnected.
// While it is 0 the firmware tracks MQTT commands but never drives an output.
#define BOARD_PINS_CONFIRMED 0

// Replace -1 with the traced GPIO for each channel, then set BOARD_PINS_CONFIRMED to 1.
#define PIN_CH1 -1
#define PIN_CH2 -1
#define PIN_CH3 -1
#define PIN_CH4 -1

// Working-status LED, if it is on a GPIO rather than the 3V3 rail. -1 = unused.
#define PIN_STATUS_LED -1

// MOC3063 LED on when the GPIO is high. Invert here if the trace shows otherwise.
#define CHANNEL_ACTIVE_LEVEL HIGH
#define CHANNEL_IDLE_LEVEL LOW
