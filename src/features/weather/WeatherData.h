// WeatherData.h — runtime (volatile) weather snapshot from open-meteo.
#pragma once
#include <Arduino.h>

struct WeatherData {
  int      temp;         // current, whole degrees (C or F per settings)
  int      high;         // today's forecast high
  int      low;          // today's forecast low
  int      rainChance;   // 0..100 %
  int      humidity;     // 0..100 %
  int      code;         // raw WMO weather_code (icon selection)
  char     condition[20]; // "Clear", "Rain", ... (weatherCodeToText)

  bool     valid;
  bool     error;
  uint32_t lastOkMs;

  void clear() {
    temp = high = low = rainChance = humidity = code = 0;
    condition[0] = 0;
    valid = false;
    error = false;
    lastOkMs = 0;
  }
};
