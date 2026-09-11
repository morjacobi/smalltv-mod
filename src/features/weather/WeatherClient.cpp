#include "WeatherClient.h"
#include "Platform.h"
#include <ArduinoJson.h>
#include <math.h>

static WeatherData g_weather;
static uint32_t    g_nextPollMs = 0;
static bool        g_inited = false;

void weatherInit(const Settings& s) {
  (void)s;
  g_weather.clear();
  g_nextPollMs = millis();
  g_inited = true;
}

void weatherForceRefresh() { g_nextPollMs = millis(); }
const WeatherData& weatherGet() { return g_weather; }
bool weatherFresh(uint32_t withinMs) {
  return g_weather.valid && (millis() - g_weather.lastOkMs) <= withinMs;
}

// WMO weather_code -> short label (open-meteo's code table).
const char* weatherCodeToText(int code) {
  switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mostly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: case 56: case 57: return "Drizzle";
    case 61: case 63: case 65: case 66: case 67: return "Rain";
    case 71: case 73: case 75: case 77: return "Snow";
    case 80: case 81: case 82: return "Showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Storm";
    default: return "Weather";
  }
}

WeatherIcon weatherCodeToIcon(int code) {
  switch (code) {
    case 0: case 1:                                 return WICON_SUN;
    case 45: case 48:                                return WICON_FOG;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82:                       return WICON_RAIN;
    case 71: case 73: case 75: case 77:
    case 85: case 86:                                return WICON_SNOW;
    case 95: case 96: case 99:                       return WICON_STORM;
    default:                                         return WICON_CLOUD;
  }
}

// ---- parse -------------------------------------------------------------
static bool applyForecast(WeatherData& d, JsonDocument& doc) {
  JsonObjectConst current = doc["current"].as<JsonObjectConst>();
  JsonObjectConst daily   = doc["daily"].as<JsonObjectConst>();
  if (current.isNull() || current["temperature_2m"].isNull()) return false;

  JsonArrayConst highs = daily["temperature_2m_max"].as<JsonArrayConst>();
  JsonArrayConst lows  = daily["temperature_2m_min"].as<JsonArrayConst>();
  JsonArrayConst rain  = daily["precipitation_probability_max"].as<JsonArrayConst>();

  d.temp = (int)lroundf(current["temperature_2m"].as<float>());
  d.high = (!highs.isNull() && highs.size()) ? (int)lroundf(highs[0].as<float>()) : d.temp;
  d.low  = (!lows.isNull()  && lows.size())  ? (int)lroundf(lows[0].as<float>())  : d.temp;
  d.rainChance = (!rain.isNull() && rain.size())
                   ? constrain((int)lroundf(rain[0].as<float>()), 0, 100) : 0;
  d.humidity = current["relative_humidity_2m"] | 0;
  d.code = current["weather_code"] | 0;
  strlcpy(d.condition, weatherCodeToText(d.code), sizeof(d.condition));

  d.valid = true;
  d.error = false;
  d.lastOkMs = millis();
  return true;
}

static void buildFilter(JsonDocument& f) {
  JsonObject current = f["current"].to<JsonObject>();
  current["temperature_2m"]      = true;
  current["weather_code"]        = true;
  current["relative_humidity_2m"] = true;
  JsonObject daily = f["daily"].to<JsonObject>();
  daily["temperature_2m_max"] = true;
  daily["temperature_2m_min"] = true;
  daily["precipitation_probability_max"] = true;
}

// ---- one plain-HTTP GET + parse ------------------------------------------
// open-meteo answers HTTP with no TLS at all: the one feed on this device with
// zero heap/handshake cost, so it can poll even when the ticker/radar can't.
static bool fetchForecast(const Settings& s) {
  String url = "http://" + String(WEATHER_HOST) + WEATHER_PATH +
               "?latitude=" + String(s.weather.lat, 4) +
               "&longitude=" + String(s.weather.lon, 4) +
               "&current=temperature_2m,weather_code,relative_humidity_2m" +
               "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max" +
               "&forecast_days=1&timezone=auto" +
               (s.weather.fahrenheit ? "&temperature_unit=fahrenheit" : "");

  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true);   // avoid chunked framing; parser reads the raw stream
  http.setTimeout(s.httpTimeout);
  http.setReuse(false);
  if (!http.begin(client, url)) return false;
  http.addHeader("Accept", "application/json");
  http.setUserAgent(F(FW_NAME));

  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  JsonDocument filter; buildFilter(filter);
  JsonDocument doc;
  bool ok = !deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))
         && applyForecast(g_weather, doc);
  http.end();
  return ok;
}

void weatherService(const Settings& s) {
  if (!g_inited) weatherInit(s);
  if (s.weather.lat == 0.0f && s.weather.lon == 0.0f) return;   // no home set
  if ((int32_t)(millis() - g_nextPollMs) < 0) return;

  if (!fetchForecast(s)) g_weather.error = true;   // keep stale data, flag it

  g_nextPollMs = millis() + (uint32_t)s.weather.pollSec * 1000UL;
}
