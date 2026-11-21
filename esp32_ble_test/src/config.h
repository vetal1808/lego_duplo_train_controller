#ifndef CONFIG_H
#define CONFIG_H

/*
 * Board: WEMOS LOLIN32 Lite (ESP32).
 * - Button grounds (outputs driven LOW): GPIO 5, 15, 16, 0
 * - Buttons 1-4 (INPUT_PULLUP, pressed = LOW): GPIO 18, 2, 17, 4
 * - Knob potentiometer: GND GPIO 25, VCC GPIO 32, ADC GPIO 33
 * - Status LED: GPIO 22 (onboard)
 * - GPIO 13 is free.
 *
 * The buttons sit on one 8-pin header run: 15 2 0 4 16 17 5 18 (ground, signal, ...).
 * GPIO 0, 2 and 15 are strapping pins: do not hold buttons 2 or 4 while powering on.
 */

#define UPDATE_INTERVAL_MS 10

// 80 MHz is the lowest clock that Bluetooth works with; it saves power against the default 240.
const uint32_t CPU_FREQ_MHZ = 240;

// Index = physical button number on the remote, minus one (checked by pressing).
// Button/ground pairs: 1 - 18/5, 2 - 2/15, 3 - 17/16, 4 - 4/0.
const int GND_PINS[4] = {5, 15, 16, 0};
const int BUTTON_PINS[4] = {18, 2, 17, 4};

const int POTENTIOMETER_GND_PIN = 25;
const int POTENTIOMETER_VCC_PIN = 32;
const int POTENTIOMETER_PIN = 33;

// Onboard LED of the LOLIN32 Lite. Set to false if it lights the wrong way round.
const int LED_PIN = 22;
const bool LED_ACTIVE_LOW = true;

// No button presses or knob movement for this long -> stop, disconnect, sleep.
const unsigned long INACTIVITY_TIMEOUT = 5 * 60 * 1000;

// Knob: raw ADC readings at the two end stops.
const int POTENTIOMETER_THRESHOLD = 10; // change (in %) that counts as moving the knob
const int POTENTIOMETER_MAX = 2500;
const int POTENTIOMETER_MIN = 1150;
const int POTENTIOMETER_OFFSET = (POTENTIOMETER_MAX + POTENTIOMETER_MIN) / 2;
const int POTENTIOMETER_SCALE = 65536 * 200 / (POTENTIOMETER_MAX - POTENTIOMETER_MIN); // -> about -100..100

// Speed from the knob.
const bool SPEED_INVERT = true; // the knob direction is reversed on this remote
const int SPEED_DEADZONE = 20;  // |knob| below this -> stop; at rest the knob reads about 10
const int SPEED_QUANTUM = 10;   // round the speed to steps so noise does not send commands
const unsigned long SPEED_SEND_INTERVAL_MS = 100;

// Button that wakes the board from deep sleep (index, 1 = button 2).
// Both of its pins must be RTC GPIOs: 0, 2, 4, 12-15, 25-27, 32-39. Buttons 2 (2/15) and 4 (4/0) qualify.
const int WAKE_BUTTON = 1;

// Button actions.
enum class ButtonAction { STOP, HORN, NEXT_LIGHT, NONE };
const ButtonAction BUTTON_ACTIONS[4] = {ButtonAction::STOP, ButtonAction::HORN, ButtonAction::NEXT_LIGHT,
                                        ButtonAction::NONE};

#endif
