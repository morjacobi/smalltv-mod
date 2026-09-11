// WeatherData.h — runtime (volatile) weather snapshot from open-meteo.
#pragma once
#include <Arduino.h>

struct WeatherData {
  int      temp;         // current, whole degrees (C or F per settings)
  int      high;         // today's forecast high
  int      low;          // today's forecast low
  int      rainChance;   // 0..100 %
  char     condition[20]; // "Clear", "Rain", ... (weatherCodeToText)

  bool     valid;
  bool     error;
  uint32_t lastOkMs;

  void clear() {
    temp = high = low = rainChance = 0;
    condition[0] = 0;
    valid = false;
    error = false;
    lastOkMs = 0;
  }
};
