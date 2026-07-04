/*
 * ESP32 Test Firmware for Button & Potentiometer Control
 * Board name WEMOS LOLIN32 Lite
 * Features:
 * - Detects button presses and logs to console
 * - Monitors potentiometer changes
 * - Auto-disables relay after 5 minutes of inactivity
 * - USB Serial output for debugging
 */

#include "button.h"
#include "pot.h"
#include "inactivity_monitor.h"
#include "config.h"
#include <Arduino.h>

//#define DEBUG_POTENTIOMETER

typedef struct
{
  Button button1;
  Button button2;
  Button button3;
  Button button4;
  Pot potentiometer;
  InactivityMonitor inactivityMonitor;
  uint32_t nextUpdateTimeMs;
} app_s;

static app_s _app;

void button1OnFall()
{
  Serial.println("Button 1 pressed");
  _app.inactivityMonitor.resetActivity();
}
void button2OnFall()
{
  Serial.println("Button 2 pressed");
  _app.inactivityMonitor.resetActivity();
}
void button3OnFall()
{
  Serial.println("Button 3 pressed");
  _app.inactivityMonitor.resetActivity();
}
void button4OnFall()
{
  Serial.println("Button 4 pressed");
  _app.inactivityMonitor.resetActivity();
}

void potentiometerOnChange()
{
  int16_t potValue = _app.potentiometer.get();
  Serial.print("Potentiometer value: ");
  Serial.println(potValue);
  _app.inactivityMonitor.resetActivity();
}

void inactivityTimeoutCallback()
{
  Serial.println("Inactivity timeout reached. Disabling relay.");
  digitalWrite(RELAY_PIN, LOW);
}

void init_buttons()
{

  // init gnd pins
  for (int i = 0; i < 4; i++)
  {
    pinMode(GND_PINS[i], OUTPUT);
    digitalWrite(GND_PINS[i], LOW);
  }

  // init buttons
  _app.button1 = Button(BUTTON_PINS[0]);
  _app.button1.setOnFall(button1OnFall);
  _app.button2 = Button(BUTTON_PINS[1]);
  _app.button2.setOnFall(button2OnFall);
  _app.button3 = Button(BUTTON_PINS[2]);
  _app.button3.setOnFall(button3OnFall);
  _app.button4 = Button(BUTTON_PINS[3]);
  _app.button4.setOnFall(button4OnFall);
}

void init_potentiometer()
{
  // init potentiometer power pins
  pinMode(POTENTIOMETER_GND_PIN, OUTPUT);
  pinMode(POTENTIOMETER_VCC_PIN, OUTPUT);
  digitalWrite(POTENTIOMETER_GND_PIN, LOW);
  digitalWrite(POTENTIOMETER_VCC_PIN, HIGH);

  _app.potentiometer = Pot(POTENTIOMETER_PIN, POTENTIOMETER_SCALE, POTENTIOMETER_OFFSET, POTENTIOMETER_THRESHOLD);
  _app.potentiometer.setOnChange(potentiometerOnChange);
}

void init_inactivity_monitor()
{
  _app.inactivityMonitor = InactivityMonitor(INACTIVITY_TIMEOUT, inactivityTimeoutCallback);
}

void debugPotentiometer()
{
  int16_t potValue = _app.potentiometer.get();
  int16_t potRawValue = analogRead(POTENTIOMETER_PIN);
  printf("Potentiometer raw value: %d, normalized value: %d\n", potRawValue, potValue);
}

void updateElements()
{
  _app.button1.update();
  _app.button2.update();
  _app.button3.update();
  _app.button4.update();
  _app.potentiometer.update();
  _app.inactivityMonitor.update();
#ifdef DEBUG_POTENTIOMETER
  debugPotentiometer();
#endif
}

void setup()
{
  // Initialize Serial for console output
  Serial.begin(115200);
  delay(2000); // Wait for serial connection to stabilize

  Serial.println("\n\n================================");
  Serial.println("ESP32 Test Firmware Started");
  Serial.println("================================\n");
  digitalWrite(RELAY_PIN, HIGH);

  init_buttons();
  init_potentiometer();
  init_inactivity_monitor();
  _app.nextUpdateTimeMs = millis();
  Serial.println("System ready! Waiting for events...\n");
}

void loop()
{
  uint32_t timeNow = millis();
  if (_app.nextUpdateTimeMs <= timeNow)
  {
    _app.nextUpdateTimeMs = timeNow + UPDATE_INTERVAL_MS;
    updateElements();
  }
}
