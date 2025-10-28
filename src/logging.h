#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef PLANTEY_DEBUG_LEVEL
#define PLANTEY_DEBUG_LEVEL 2
#endif

namespace logging {

inline void logMessage(uint8_t level, const char* tag, const char* fmt, ...) {
  if (level > PLANTEY_DEBUG_LEVEL) {
    return;
  }
  if (!Serial) {
    return;
  }

  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  Serial.printf("[%8lu] [%s] %s\n", static_cast<unsigned long>(millis()), tag != nullptr ? tag : "log", buffer);
}

}  // namespace logging

#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4

#define LOG_ERROR(tag, fmt, ...) logging::logMessage(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...) logging::logMessage(LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...) logging::logMessage(LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...) logging::logMessage(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
