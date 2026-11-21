#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

// One log line: "[millis][tag] message". The line is built in full and written
// with a single call, so lines from different tasks never interleave.
void logLine(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// "01 AB FF " into out; stops early if out is too small.
void toHex(const uint8_t* data, size_t len, char* out, size_t outSize);

#endif
