"""Firmware check without a train: scanning, no blocking, falling into deep sleep.

Waking with a button cannot be checked from here: at the end the board sleeps,
press button 2 to wake it.

Run with the train switched off and the serial monitor closed:
  ~/.platformio/penv/Scripts/python.exe tools/no_train_test.py --port COM9
"""

import argparse
import re
import sys
import time

import serial

BOOT_BANNER = "DUPLO controller build"
MAX_GAP_LIMIT_US = 30000
FAKE_CONNECT_LIMIT_S = 11.0
FAKE_CONNECT_ATTEMPTS = 10
MIN_SCAN_CYCLES = 3
SEARCH_TIMEOUT_S = 120


class Board:
    def __init__(self, port, log_path):
        self.port = serial.Serial()
        self.port.port = port
        self.port.baudrate = 115200
        self.port.timeout = 0.1
        self.port.dtr = False
        self.port.rts = False
        self.port.open()
        self.lines = []
        self.buffer = b""
        self.log = open(log_path, "w", encoding="utf-8") if log_path else None

    def reset(self):
        self.port.rts = True
        time.sleep(0.1)
        self.port.rts = False

    def poll(self):
        """Reads the port and returns the new complete lines."""
        self.buffer += self.port.read(4096)
        *complete, self.buffer = self.buffer.split(b"\n")
        new = [line.decode("utf-8", "replace").rstrip() for line in complete]
        for line in new:
            self.lines.append(line)
            if self.log:
                self.log.write(line + "\n")
                self.log.flush()
        return new

    def wait_for(self, text, timeout):
        """Waits for a line containing text; returns it, or None on timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for line in self.poll():
                if text in line:
                    return line
        return None

    def collect(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.poll()

    def send(self, command):
        self.port.write(command.encode())


def heartbeats(lines):
    result = []
    for line in lines:
        match = re.search(r"state=(\w+) .*max_gap=(\d+) us heap=(\d+) dropped=(\d+)", line)
        if match:
            result.append((match[1], int(match[2]), int(match[3]), int(match[4])))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--log", default=None, help="where to save the full log")
    args = parser.parse_args()

    board = Board(args.port, args.log)
    failures = []

    def check(ok, message):
        print(f"{'PASS' if ok else 'FAIL'}: {message}", flush=True)
        if not ok:
            failures.append(message)

    board.reset()
    started = time.monotonic()

    check(board.wait_for(BOOT_BANNER, 5) is not None, "firmware started")
    check(board.wait_for("BOOT -> SEARCHING", 5) is not None, "BOOT -> SEARCHING")

    board.collect(20)
    cycles = sum("scan cycle #" in line for line in board.lines)
    check(cycles >= MIN_SCAN_CYCLES, f"scan cycles in 20 s: {cycles}, need >= {MIN_SCAN_CYCLES}")
    devices = sum(" dev " in line for line in board.lines)
    print(f"INFO: device sightings: {devices}", flush=True)

    durations = []
    first_fake_line = len(board.lines)
    for _ in range(FAKE_CONNECT_ATTEMPTS):
        board.send("f")
        sent = time.monotonic()
        line = board.wait_for("(link failed)", FAKE_CONNECT_LIMIT_S + 5)
        durations.append(time.monotonic() - sent if line else None)
        board.collect(1.5)  # pause before the next scan
    finished = [d for d in durations if d is not None]
    check(len(finished) == FAKE_CONNECT_ATTEMPTS,
          f"fake connect failed cleanly {len(finished)}/{FAKE_CONNECT_ATTEMPTS} times")
    if finished:
        check(max(finished) <= FAKE_CONNECT_LIMIT_S,
              f"longest attempt {max(finished):.1f} s, limit {FAKE_CONNECT_LIMIT_S} s")
    fake_beats = heartbeats(board.lines[first_fake_line:])
    connecting_beats = [b for b in fake_beats if b[0] == "CONNECTING"]
    check(len(connecting_beats) > 0, f"heartbeat keeps running during CONNECTING: {len(connecting_beats)}")

    board.send("s")
    board.wait_for("status:", 5)
    remaining = SEARCH_TIMEOUT_S - (time.monotonic() - started) + 10
    check(board.wait_for("-> SLEEP (search timeout)", max(remaining, 10)) is not None, "search timeout -> SLEEP")
    check(board.wait_for("deep sleep now", 6) is not None, "SLEEP -> deep sleep after 3 s")
    board.collect(10)
    after_sleep = board.lines[-1] if board.lines else ""
    check("deep sleep now" in after_sleep, "board stays silent for 10 s after sleeping (no reboot)")

    beats = heartbeats(board.lines)
    if beats:
        worst = max(b[1] for b in beats)
        check(worst < MAX_GAP_LIMIT_US, f"max_gap over the whole test {worst} us, limit {MAX_GAP_LIMIT_US} us")
        print(f"INFO: heap {beats[0][2]} -> {beats[-1][2]}, minimum {min(b[2] for b in beats)}", flush=True)
        print(f"INFO: dropped events: {beats[-1][3]}", flush=True)
    else:
        check(False, "no heartbeat")

    boots = sum(BOOT_BANNER in line for line in board.lines)
    check(boots == 1, f"firmware starts during the test: {boots}, expected 1 (no reboots)")

    print(f"\n{'FAILED: ' + str(len(failures)) if failures else 'ALL CHECKS PASSED'}", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
