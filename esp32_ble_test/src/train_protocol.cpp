#include "train_protocol.h"

#include <Arduino.h>
#include <initializer_list>

#include "log.h"

namespace lwp {

static Command makeCommand(const char* name, std::initializer_list<uint8_t> bytes) {
  Command cmd = {};
  strlcpy(cmd.name, name, sizeof(cmd.name));
  for (uint8_t b : bytes) {
    if (cmd.length < sizeof(cmd.bytes)) {
      cmd.bytes[cmd.length++] = b;
    }
  }
  return cmd;
}

Command speed(int percent) {
  percent = constrain(percent, -100, 100);
  if (percent == 0) {
    // 0x7F = float: the motor is released, same as stop() in duplo_train.py.
    return makeCommand("stop", {0x08, 0x00, 0x81, PORT_MOTOR, 0x11, 0x51, 0x00, 0x7F});
  }
  char name[24];
  snprintf(name, sizeof(name), "speed %d%%", percent);
  return makeCommand(name, {0x09, 0x00, 0x81, PORT_MOTOR, 0x11, 0x07, (uint8_t)(int8_t)percent, 0x64, 0x03});
}

Command horn() {
  return makeCommand("horn", {0x0B, 0x00, 0x81, PORT_SOUND_LIGHT, 0x11, 0x51, 0x01, 0x07, 0x01, 0x00, 0x00});
}

Command light(uint8_t color) {
  char name[24];
  snprintf(name, sizeof(name), "light 0x%02X", color);
  return makeCommand(name, {0x0B, 0x00, 0x81, PORT_SOUND_LIGHT, 0x11, 0x51, 0x01, 0x04, 0x01, color, 0x00});
}

Command subscribePort(uint8_t port) {
  char name[24];
  snprintf(name, sizeof(name), "subscribe port 0x%02X", port);
  return makeCommand(name, {0x0A, 0x00, 0x41, port, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01});
}

void logNotification(const char* tag, const uint8_t* data, size_t length, size_t fullLength) {
  if (length >= 5 && data[2] == MSG_PORT_VALUE) {
    // Sensor values arrive often, so they get one short line without hex.
    uint8_t port = data[3];
    if (port == PORT_COLOR) {
      logLine(tag, "color tile: id=0x%02X", data[4]);
    } else if (port == PORT_SPEED_SENSOR) {
      logLine(tag, "speed sensor: %d", (int8_t)data[4]);
    } else if (port == PORT_BATTERY && length >= 6) {
      logLine(tag, "battery: %u mV", (unsigned)(data[4] | (data[5] << 8)));
    } else {
      logLine(tag, "value on port 0x%02X", port);
    }
    return;
  }

  char hex[32 * 3 + 1];
  toHex(data, length, hex, sizeof(hex));
  logLine(tag, "rx len=%u: %s%s", (unsigned)fullLength, hex, fullLength > length ? "(truncated)" : "");
  if (length < 5) {
    return;
  }
  uint8_t type = data[2];
  uint8_t port = data[3];
  if (type == MSG_ATTACHED_IO) {
    logLine(tag, "  attached IO: port=0x%02X event=0x%02X", port, data[4]);
  } else if (type == MSG_PORT_INPUT_ACK) {
    logLine(tag, "  port subscription ack: port=0x%02X", port);
  } else {
    logLine(tag, "  message type 0x%02X", type);
  }
}

} // namespace lwp
