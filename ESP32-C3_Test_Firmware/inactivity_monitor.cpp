#include "inactivity_monitor.h"
#include <Arduino.h>

InactivityMonitor::InactivityMonitor(unsigned long timeout, InactivityCallback callback)
    : inactivityTimeout(timeout), onTimeoutCallback(callback), hasTimedOut(false) {
  lastActivityTime = millis();
}

void InactivityMonitor::resetActivity() {
  lastActivityTime = millis();
  hasTimedOut = false;
}

void InactivityMonitor::setTimeout(unsigned long timeout) {
  inactivityTimeout = timeout;
}

void InactivityMonitor::update() {
  unsigned long currentTime = millis();
  unsigned long inactiveTime = currentTime - lastActivityTime;

  if (inactiveTime >= inactivityTimeout && !hasTimedOut) {
    hasTimedOut = true;
    if (onTimeoutCallback != nullptr) {
      onTimeoutCallback();
    }
  }
}

unsigned long InactivityMonitor::getInactiveTime() {
  return millis() - lastActivityTime;
}

bool InactivityMonitor::isTimedOut() {
  return hasTimedOut;
}

void InactivityMonitor::resetTimeout() {
  hasTimedOut = false;
}
