/*
 * ESP32 Test Firmware for Button & Potentiometer Control
 * 
 * Hardware Configuration:
 * - GND Source Pins (outputs LOW): GPIO 13, 2, 4, 17
 * - Button Inputs (pull-down): GPIO 15, 0, 16, 5 (Buttons 1-4)
 * - Potentiometer GND: GPIO 32
 * - Potentiometer VCC: GPIO 34
 * - Potentiometer ADC: GPIO 35
 * - Power Relay: GPIO 23
 * 
 * Features:
 * - Detects button presses and logs to console
 * - Monitors potentiometer changes
 * - Auto-disables relay after 5 minutes of inactivity
 * - USB Serial output for debugging
 */

#include "button.h"
#include "pot.h"
#include "config.h"
#include <Arduino.h>
typedef struct{
  Button button1;
  Button button2;
  Button button3;
  Button button4;
  Pot potentiometer;
  uint32_t lastActivityTime;
}app_s;

static app_s _app;


// State Variables
unsigned long lastEventTime = 0;                   // Timestamp of last event
bool relayActive = true;                           // Current relay state
int lastPotentiometerValue = 0;                    // Previous potentiometer reading
bool buttonStates[4] = {false, false, false, false};  // Current button states
bool prevButtonStates[4] = {false, false, false, false};  // Previous button states

void init_buttons() {

    //init gnd pins
    for (int i = 0; i < 4; i++) {
        pinMode(GND_PINS[i], OUTPUT);
        digitalWrite(GND_PINS[i], LOW);
    }

    //init buttons
    _app.button1 = Button(BUTTON_PINS[0]);
    _app.button2 = Button(BUTTON_PINS[1]);
    _app.button3 = Button(BUTTON_PINS[2]);
    _app.button4 = Button(BUTTON_PINS[3]);
}

void init_potentiometer() {
    //init potentiometer power pins
    pinMode(POTENTIOMETER_GND_PIN, OUTPUT);
    pinMode(POTENTIOMETER_VCC_PIN, OUTPUT);
    digitalWrite(POTENTIOMETER_GND_PIN, LOW);
    digitalWrite(POTENTIOMETER_VCC_PIN, HIGH);

    _app.potentiometer = Pot(POTENTIOMETER_PIN, 1, 0, POTENTIOMETER_THRESHOLD);
}


void setup() {
  // Initialize Serial for console output
  Serial.begin(115200);
  delay(2000);  // Wait for serial connection to stabilize
  
  Serial.println("\n\n================================");
  Serial.println("ESP32 Test Firmware Started");
  Serial.println("================================\n");
  
  init_buttons();
  init_potentiometer();
 
  Serial.println("System ready! Waiting for events...\n");
}

void loop() {
 
}
