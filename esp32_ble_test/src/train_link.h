#ifndef TRAIN_LINK_H
#define TRAIN_LINK_H

#include <stdint.h>

// Bluetooth link to the train: a state machine running in its own FreeRTOS task.
// Every blocking NimBLE call happens in that task, so loop() never waits on Bluetooth.
//
//   BOOT -> SEARCHING -> CONNECTING -> WAIT_RESPONSE -> CONNECTED
//   any failure or lost link -> RECONNECTING; search timeout or shutdown -> SLEEP

enum class LinkState : uint8_t { BOOT, SEARCHING, CONNECTING, WAIT_RESPONSE, CONNECTED, RECONNECTING, SLEEP };

// Commands for linkCommand(). The same characters work over UART.
namespace LinkCmd {
constexpr char STATUS       = 's';
constexpr char DISCONNECT   = 'd';
constexpr char WAKE         = 'w'; // SLEEP -> SEARCHING
constexpr char HORN         = 'h';
constexpr char NEXT_LIGHT   = 'c';
constexpr char FAKE_CONNECT = 'f'; // connect to a missing address: checks that nothing blocks
constexpr char SLEEP_NOW    = 'z'; // stop, disconnect, SLEEP
constexpr char SHUTDOWN     = 'x'; // same, sent on inactivity
} // namespace LinkCmd

void linkStart();

LinkState linkState();
const char* linkStateName(LinkState state);
uint32_t linkStateSinceMs();

// Thread-safe: queues the command for the link task.
void linkCommand(char cmd);

// Speed the train should run at; the link task sends it while CONNECTED.
void linkSetDesiredSpeed(int speed);
int linkDesiredSpeed();
void linkForceSpeedSend(); // resend even if unchanged (stop button)

uint32_t linkDroppedEvents();

#endif
