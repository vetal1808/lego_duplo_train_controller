/*
 * Hardware Configuration:
 * - GND Source Pins (outputs LOW): GPIO 13, 2, 4, 17
 * - Button Inputs (pull-down): GPIO 15, 0, 16, 5 (Buttons 1-4)
 * - Potentiometer GND: GPIO 32
 * - Potentiometer VCC: GPIO 34
 * - Potentiometer ADC: GPIO 35
 * - Power Relay: GPIO 23
 */

#define UPDATE_INTERVAL_MS 10

const int GND_PINS[4] = {13, 2, 4, 17}; // GPIO pins configured as GND source (output LOW)

// Pin Configuration - Buttons
const int BUTTON_PINS[4] = {15, 0, 16, 5}; // GPIO pins for 4 buttons (pull-down)

// Pin Configuration - Potentiometer
const int POTENTIOMETER_GND_PIN = 25; // GPIO pin_name for potentiometer GND
const int POTENTIOMETER_VCC_PIN = 32; // GPIO pin_name for potentiometer VCC
const int POTENTIOMETER_PIN = 33;     // GPIO pin_name for analog potentiometer input

// Pin Configuration - Relay
const int RELAY_PIN = 23; // GPIO pin_name for power relay

// Timing Configuration
const unsigned long INACTIVITY_TIMEOUT = 5 * 60 * 1000; // 5 minutes in milliseconds

// Button debounce configuration
const int DEBOUNCE_DELAY = 50; // milliseconds
const int POTENTIOMETER_THRESHOLD = 10;

const int POTENTIOMETER_MAX = 2559;
const int POTENTIOMETER_MIN = 640;
const int POTENTIOMETER_OFFSET = (POTENTIOMETER_MAX + POTENTIOMETER_MIN) / 2;                                                 // Offset for potentiometer reading
const int POTENTIOMETER_SCALE = 65536 * 200 / (POTENTIOMETER_MAX - POTENTIOMETER_MIN); // Scale ADC value (0-4095) to percentage (0-100%)
