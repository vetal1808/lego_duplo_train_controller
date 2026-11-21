#include "controls.h"

#include <Arduino.h>

#include "button.h"
#include "config.h"
#include "inactivity_monitor.h"
#include "log.h"
#include "pot.h"
#include "train_link.h"

static const uint32_t WAKE_PENDING_MS = 1000; // time for the link task to leave SLEEP
static const int NOT_LOGGED = 1000;

static Button g_buttons[4];
static Pot g_pot;
static InactivityMonitor g_inactivity;
static bool g_interlock = true; // waiting for the knob at zero before driving
static int g_lastLoggedSpeed = NOT_LOGGED;
static uint32_t g_wakeRequestedMs = 0;
static uint32_t g_knobLogUntilMs = 0; // calibration: log raw knob readings until then
static const uint32_t KNOB_LOG_PERIOD_MS = 100;

static void setLed(bool on) { digitalWrite(LED_PIN, on != LED_ACTIVE_LOW ? HIGH : LOW); }

// Knob -> speed: dead zone in the middle, then 10..100 in SPEED_QUANTUM steps.
static int knobToSpeed(int knob) {
  knob = constrain(knob, -100, 100);
  int magnitude = abs(knob);
  if (magnitude < SPEED_DEADZONE) {
    return 0;
  }
  int speed = (magnitude - SPEED_DEADZONE) * 100 / (100 - SPEED_DEADZONE);
  speed = (speed + SPEED_QUANTUM / 2) / SPEED_QUANTUM * SPEED_QUANTUM;
  speed = constrain(speed, SPEED_QUANTUM, 100);
  return knob > 0 ? speed : -speed;
}

static void onButton(int index) {
  g_inactivity.resetActivity();
  ButtonAction action = BUTTON_ACTIONS[index];
  const char* names[] = {"stop", "horn", "next light", "none"};
  logLine("UI", "button %d pressed: %s", index + 1, names[(int)action]);

  if (linkState() == LinkState::SLEEP) {
    controlsWake();
    return; // a press while asleep only wakes
  }
  switch (action) {
    case ButtonAction::STOP:       controlsStop("button"); break;
    case ButtonAction::HORN:       linkCommand(LinkCmd::HORN); break;
    case ButtonAction::NEXT_LIGHT: linkCommand(LinkCmd::NEXT_LIGHT); break;
    case ButtonAction::NONE:       break;
  }
}

static void onButton1() { onButton(0); }
static void onButton2() { onButton(1); }
static void onButton3() { onButton(2); }
static void onButton4() { onButton(3); }

static void onPotChange() { g_inactivity.resetActivity(); }

static void onInactivity() {
  logLine("UI", "inactivity %lu s: stop, disconnect, sleep", (unsigned long)(INACTIVITY_TIMEOUT / 1000));
  g_interlock = true;
  linkSetDesiredSpeed(0);
  linkCommand(LinkCmd::SHUTDOWN);
}

void controlsInit() {
  for (int i = 0; i < 4; i++) {
    pinMode(GND_PINS[i], OUTPUT);
    digitalWrite(GND_PINS[i], LOW);
  }
  void (*callbacks[4])() = {onButton1, onButton2, onButton3, onButton4};
  for (int i = 0; i < 4; i++) {
    g_buttons[i] = Button(BUTTON_PINS[i]);
    g_buttons[i].setOnFall(callbacks[i]);
  }

  pinMode(POTENTIOMETER_GND_PIN, OUTPUT);
  pinMode(POTENTIOMETER_VCC_PIN, OUTPUT);
  digitalWrite(POTENTIOMETER_GND_PIN, LOW);
  digitalWrite(POTENTIOMETER_VCC_PIN, HIGH);
  g_pot = Pot(POTENTIOMETER_PIN, POTENTIOMETER_SCALE, POTENTIOMETER_OFFSET, POTENTIOMETER_THRESHOLD);
  g_pot.setOnChange(onPotChange);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  g_inactivity = InactivityMonitor(INACTIVITY_TIMEOUT, onInactivity);
}

void controlsUpdate() {
  for (auto& button : g_buttons) {
    button.update();
  }
  g_pot.update();
  g_inactivity.update();

  // Every new connection starts stopped, waiting for the knob at zero.
  static LinkState prevState = LinkState::BOOT;
  LinkState state = linkState();
  if (state == LinkState::CONNECTED && prevState != LinkState::CONNECTED) {
    g_interlock = true;
    logLine("UI", "connected: return the knob to zero to drive");
  }
  prevState = state;

  static uint32_t lastKnobLogMs = 0;
  uint32_t now = millis();
  if ((int32_t)(g_knobLogUntilMs - now) > 0 && now - lastKnobLogMs >= KNOB_LOG_PERIOD_MS) {
    lastKnobLogMs = now;
    logLine("KNOB", "raw=%u knob=%d", g_pot.getRaw(), g_pot.get());
  }

  int knobSpeed = knobToSpeed(SPEED_INVERT ? -g_pot.get() : g_pot.get());
  if (g_interlock && knobSpeed == 0) {
    g_interlock = false;
    logLine("UI", "knob at zero: driving enabled");
  }
  int desired = g_interlock ? 0 : knobSpeed;
  linkSetDesiredSpeed(desired);
  if (desired != g_lastLoggedSpeed) {
    logLine("UI", "speed request %d%% (knob %d)", desired, g_pot.get());
    g_lastLoggedSpeed = desired;
  }
}

// Fast blink: searching or connecting. Steady: connected.
// Short flashes: about to sleep. Off: deep sleep.
void controlsUpdateLed(uint32_t now) {
  bool on;
  switch (linkState()) {
    case LinkState::CONNECTED: on = true; break;
    case LinkState::SLEEP:     on = now % 1000 < 50; break;
    default:                   on = now % 300 < 150; break;
  }
  setLed(on);
}

void controlsStop(const char* source) {
  g_interlock = true;
  linkSetDesiredSpeed(0);
  linkForceSpeedSend();
  logLine("UI", "STOP (%s): return the knob to zero to drive again", source);
}

void controlsWake() {
  g_inactivity.resetActivity();
  if (linkState() == LinkState::SLEEP) {
    g_wakeRequestedMs = millis();
    linkCommand(LinkCmd::WAKE);
  }
}

void controlsNoteActivity() { g_inactivity.resetActivity(); }

int controlsKnob() { return g_pot.get(); }

void controlsLogKnob(uint32_t durationMs) {
  logLine("KNOB", "logging raw knob readings for %lu s", (unsigned long)(durationMs / 1000));
  g_knobLogUntilMs = millis() + durationMs;
}

bool controlsWakePending(uint32_t now) {
  return g_wakeRequestedMs != 0 && now - g_wakeRequestedMs < WAKE_PENDING_MS;
}

void controlsLedOff() { setLed(false); }
