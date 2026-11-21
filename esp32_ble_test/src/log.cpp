#include "log.h"

#include <Arduino.h>
#include <cstdarg>

void logLine(const char* tag, const char* fmt, ...) {
  char text[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  char line[240];
  int n = snprintf(line, sizeof(line), "[%8lu][%-13s] %s\r\n", (unsigned long)millis(), tag, text);
  if (n > 0) {
    Serial.write(reinterpret_cast<const uint8_t*>(line), min<size_t>(n, sizeof(line) - 1));
  }
}

void toHex(const uint8_t* data, size_t len, char* out, size_t outSize) {
  size_t pos = 0;
  out[0] = '\0';
  for (size_t i = 0; i < len && pos + 4 <= outSize; i++) {
    pos += snprintf(out + pos, outSize - pos, "%02X ", data[i]);
  }
}
