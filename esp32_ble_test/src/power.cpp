#include "power.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "config.h"
#include "controls.h"
#include "log.h"
#include "train_link.h"

static const uint32_t DEEP_SLEEP_DELAY_MS = 3000;

// Pins switched to RTC mode for sleep; they go back to normal GPIO on wake.
static const gpio_num_t RTC_SLEEP_PINS[] = {(gpio_num_t)BUTTON_PINS[WAKE_BUTTON], (gpio_num_t)GND_PINS[WAKE_BUTTON],
                                            (gpio_num_t)POTENTIOMETER_VCC_PIN};

static void rtcOutputLow(gpio_num_t pin) {
  rtc_gpio_init(pin);
  rtc_gpio_set_direction(pin, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(pin, 0);
  rtc_gpio_hold_en(pin);
}

static void enterDeepSleep() {
  gpio_num_t button = (gpio_num_t)BUTTON_PINS[WAKE_BUTTON];
  logLine("SLEEP", "deep sleep now, wake: button %d (GPIO %d)", WAKE_BUTTON + 1, (int)button);
  Serial.flush();

  // Digital pins (LED, button grounds) keep their level for the whole sleep.
  controlsLedOff();
  gpio_deep_sleep_hold_en();

  // Unpower the knob, otherwise ~330 uA flows through it.
  rtcOutputLow((gpio_num_t)POTENTIOMETER_VCC_PIN);
  // Wake button: its ground is an RTC output held LOW, the button an RTC input with pull-up.
  rtcOutputLow((gpio_num_t)GND_PINS[WAKE_BUTTON]);
  rtc_gpio_init(button);
  rtc_gpio_set_direction(button, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis(button);
  rtc_gpio_pullup_en(button);
  esp_sleep_enable_ext0_wakeup(button, 0);

  esp_deep_sleep_start();
}

void powerTick(uint32_t now) {
  if (linkState() != LinkState::SLEEP || now - linkStateSinceMs() < DEEP_SLEEP_DELAY_MS) {
    return;
  }
  if (controlsWakePending(now)) {
    return; // a button was pressed, the link task has not left SLEEP yet
  }
  if (digitalRead(BUTTON_PINS[WAKE_BUTTON]) == LOW) {
    return; // ext0 is level-triggered: a held button would wake the board at once
  }
  enterDeepSleep();
}

void powerReleaseSleepPins() {
  gpio_deep_sleep_hold_dis();
  for (gpio_num_t pin : RTC_SLEEP_PINS) {
    rtc_gpio_hold_dis(pin);
    rtc_gpio_deinit(pin);
  }
}

void powerLogWakeCause() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    logLine("BOOT", "woke from deep sleep by button %d", WAKE_BUTTON + 1);
  } else {
    logLine("BOOT", "wake cause=%d (0 = power on or reset)", (int)cause);
  }
}
