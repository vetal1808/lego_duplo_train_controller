#ifndef TRAIN_PROTOCOL_H
#define TRAIN_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// LEGO Wireless Protocol 3 as spoken by the DUPLO train hub.
// Command bytes match duplo_train.py, where each was tested on the train.
namespace lwp {

constexpr const char* SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
constexpr const char* CHAR_UUID    = "00001624-1212-efde-1623-785feabcd123";

constexpr uint8_t PORT_MOTOR        = 0x32;
constexpr uint8_t PORT_COLOR        = 0x33;
constexpr uint8_t PORT_SOUND_LIGHT  = 0x34;
constexpr uint8_t PORT_BATTERY      = 0x35;
constexpr uint8_t PORT_SPEED_SENSOR = 0x36;

constexpr uint8_t MSG_ATTACHED_IO    = 0x04;
constexpr uint8_t MSG_PORT_VALUE     = 0x45;
constexpr uint8_t MSG_PORT_INPUT_ACK = 0x47;

// Headlight colour codes run from 0 to LIGHT_COLOR_COUNT - 1.
constexpr uint8_t LIGHT_COLOR_COUNT = 11;

struct Command {
  uint8_t bytes[12];
  uint8_t length;
  char name[24]; // for the log
};

Command speed(int percent); // -100..100; 0 releases the motor (float stop)
Command horn();
Command light(uint8_t color);
Command subscribePort(uint8_t port); // ask the hub to report values of this port

// Logs a notification from the hub: sensor values in short form, everything else as hex.
void logNotification(const char* tag, const uint8_t* data, size_t length, size_t fullLength);

} // namespace lwp

#endif
