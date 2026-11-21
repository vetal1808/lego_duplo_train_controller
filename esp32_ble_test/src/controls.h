#ifndef CONTROLS_H
#define CONTROLS_H

#include <stdint.h>

// Buttons, throttle knob, inactivity timer and the status LED. Runs in loop().
//
// Safety interlock: after every connect, and after Stop, the train does not
// move until the knob has been back in the dead zone.

void controlsInit();
void controlsUpdate();              // call every UPDATE_INTERVAL_MS
void controlsUpdateLed(uint32_t now);

void controlsStop(const char* source);
void controlsWake();                // leaves SLEEP; counts as activity
void controlsNoteActivity();        // restarts the inactivity timer

int controlsKnob();                 // knob value, about -100..100
void controlsLogKnob(uint32_t durationMs); // calibration: log raw ADC readings
bool controlsWakePending(uint32_t now); // a wake was just requested
void controlsLedOff();

#endif
