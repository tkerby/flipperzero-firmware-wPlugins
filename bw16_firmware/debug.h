#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

//Uncomment to enable debug messages
#define DEBUG


#ifdef DEBUG
  #define DEBUG_SER_PRINT(...) Serial.print(__VA_ARGS__);
  #define DEBUG_SER_PRINTLN(...) Serial.println(__VA_ARGS__);
#else
  #define DEBUG_SER_PRINT(...)
  #define DEBUG_SER_PRINTLN(...)
#endif

#define FUNCTION_PERF() PerfTimer perfTimer(__FUNCTION__)

#endif
