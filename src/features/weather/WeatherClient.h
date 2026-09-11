// WeatherClient.h — open-meteo forecast, plain HTTP (no key, no TLS).
//
// Ported from iodn/geekmagic-tv-esp8266 (MIT): api.open-meteo.com answers plain
// HTTP, so this is the one feed on the device with zero TLS heap cost — safe to
// run alongside the ticker/radar/music heap budget.
#pragma once
#include "Mode.h"
#include "WeatherData.h"

void weatherInit(const Settings& s);
void weatherService(const Settings& s);      // call every loop tick
void weatherForceRefresh();
const WeatherData& weatherGet();
bool weatherFresh(uint32_t withinMs);

const char* weatherCodeToText(int code);     // WMO weather_code -> short label

enum WeatherIcon : uint8_t { WICON_SUN, WICON_CLOUD, WICON_RAIN, WICON_SNOW, WICON_STORM, WICON_FOG };
WeatherIcon weatherCodeToIcon(int code);
