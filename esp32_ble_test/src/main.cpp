/*
  DUPLO train remote on ESP32 (LOLIN32 Lite).

  The knob sets speed and direction. Buttons: 1 stop, 2 horn, 3 next headlight
  colour, 4 reserved (see config.h).

  Modules:
    train_link      Bluetooth state machine in its own task; all blocking NimBLE calls
    train_protocol  LWP3 command bytes and notification decoding
    controls        buttons, knob, safety interlock, inactivity, status LED
    power           deep sleep and wake on button 2
    log             UART log lines

  loop() never blocks: it reads the controls, handles UART and prints a heartbeat
  every 5 s with the longest loop gap (max_gap).

  UART commands: s status, d disconnect, w cancel sleep, z sleep now, 0 stop,
                 h horn, c next light, k log raw knob readings for 20 s (calibration),
                 f connect to a missing address (blocking test).
*/

#include <Arduino.h>

#include "config.h"
#include "controls.h"
#include "log.h"
#include "power.h"
#include "train_link.h"

static const uint32_t HEARTBEAT_MS     = 5000;
static const uint32_t STUCK_WARNING_MS = 20000; // CONNECTING should never last this long

static void handleSerial() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c == '\r' || c == '\n') {
      continue;
    }
    if (c == '0') {
      controlsNoteActivity();
      controlsStop("uart");
    } else if (c == LinkCmd::WAKE) {
      controlsWake();
    } else if (c == 'k') {
      controlsNoteActivity();
      controlsLogKnob(20000);
    } else {
      linkCommand((char)c);
    }
  }
}

void setup() {
  setCpuFrequencyMhz(CPU_FREQ_MHZ); // before Serial and Bluetooth start
  Serial.setTxBufferSize(4096);     // writing the log must not stall the tasks
  Serial.begin(115200);
  delay(300);
  logLine("BOOT", "DUPLO controller build " __DATE__ " " __TIME__);
  logLine("BOOT", "cpu %lu MHz", (unsigned long)getCpuFrequencyMhz());
  powerLogWakeCause();

  powerReleaseSleepPins();
  controlsInit();
  linkStart();
}

void loop() {
  static uint32_t lastPassUs = micros();
  static uint32_t windowMaxGapUs = 0;
  static uint32_t lastBeatMs = millis();
  static uint32_t lastControlsMs = millis();

  uint32_t gapUs = micros() - lastPassUs;
  if (gapUs > windowMaxGapUs) {
    windowMaxGapUs = gapUs;
  }

  uint32_t now = millis();
  if (now - lastControlsMs >= UPDATE_INTERVAL_MS) {
    lastControlsMs = now;
    controlsUpdate();
  }

  handleSerial();
  controlsUpdateLed(now);
  powerTick(now);

  if (now - lastBeatMs >= HEARTBEAT_MS) {
    lastBeatMs = now;
    LinkState state = linkState();
    uint32_t inState = now - linkStateSinceMs();
    logLine("HB", "state=%s in_state=%lu ms speed_req=%d knob=%d max_gap=%lu us heap=%lu dropped=%lu",
            linkStateName(state), (unsigned long)inState, linkDesiredSpeed(), controlsKnob(),
            (unsigned long)windowMaxGapUs, (unsigned long)ESP.getFreeHeap(), (unsigned long)linkDroppedEvents());
    if (state == LinkState::CONNECTING && inState >= STUCK_WARNING_MS) {
      logLine("HB", "WARNING: CONNECTING longer than %lu ms", (unsigned long)STUCK_WARNING_MS);
    }
    windowMaxGapUs = 0;
  }

  // Printing time is excluded, so max_gap shows only stalls caused by Bluetooth.
  lastPassUs = micros();
  delay(1);
}
