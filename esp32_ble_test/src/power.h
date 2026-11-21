#ifndef POWER_H
#define POWER_H

#include <stdint.h>

// Deep sleep. DEEP_SLEEP_DELAY_MS after the link enters SLEEP (time to cancel
// with any button or 'w'), the board goes to deep sleep. Only WAKE_BUTTON
// wakes it, and the firmware then starts from scratch.

void powerLogWakeCause();
void powerReleaseSleepPins(); // first thing in setup(): undo the pin holds from sleep
void powerTick(uint32_t now); // from loop()

#endif
