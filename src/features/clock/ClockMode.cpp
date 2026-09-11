#include "ClockMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "Clock.h"
#include "WeatherClient.h"

ClockMode g_clockMode;

#define C_DIM gfxTint(0xB574)

static const char* kWeekday[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                  "Thursday", "Friday", "Saturday"};
static const char* kMonth[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void drawSynced(struct tm& t, const Settings& s) {
  Arduino_GFX* gfx = gfxDev();
  gfx->fillScreen(C_BLACK);

  char date[24];
  snprintf(date, sizeof(date), "%s, %s %d", kWeekday[t.tm_wday], kMonth[t.tm_mon], t.tm_mday);
  gfxDrawCentered(date, 22, 2, C_DIM);

  char hm[8];
  snprintf(hm, sizeof(hm), "%02d:%02d", t.tm_hour, t.tm_min);
  gfx->setTextSize(7);
  gfx->setTextColor(C_WHITE);
  int w = gfxTextW(hm, 7);
  gfx->setCursor((TFT_WIDTH - w) / 2, 78);
  gfx->print(hm);

  const WeatherData& w2 = weatherGet();
  if (w2.valid) {
    char line1[32], line2[32];
    const char* unit = s.weather.fahrenheit ? "F" : "C";
    snprintf(line1, sizeof(line1), "%d%s  %s", w2.temp, unit, w2.condition);
    snprintf(line2, sizeof(line2), "H:%d  L:%d", w2.high, w2.low);
    gfxDrawCentered(line1, 168, 2, C_WHITE);
    gfxDrawCentered(line2, 194, 2, C_DIM);
  } else if (s.weather.lat != 0.0f || s.weather.lon != 0.0f) {
    gfxDrawCentered(w2.error ? "weather error" : "weather...", 178, 2, C_DIM);
  }
}

void ClockMode::begin(const Settings& s) {
  weatherInit(s);
  needRender_ = true;
  lastMinute_ = -1;
  lastWeatherOk_ = 0xFFFFFFFF;
  lastSynced_ = false;
}

void ClockMode::invalidate(const Settings& s) { needRender_ = true; }

void ClockMode::service(const Settings& s) {
  weatherService(s);   // no TLS, cheap — fine to poll only while this screen is up

  struct tm t;
  bool synced = clockNow(t);

  bool minuteChanged = synced && t.tm_min != lastMinute_;
  bool weatherChanged = weatherGet().lastOkMs != lastWeatherOk_;
  bool syncChanged = synced != lastSynced_;

  if (!needRender_ && !minuteChanged && !weatherChanged && !syncChanged) return;

  Arduino_GFX* gfx = gfxDev();
  if (!synced) {
    gfx->fillScreen(C_BLACK);
    gfxDrawCentered("syncing clock...", 116, 2, C_DIM);
  } else {
    drawSynced(t, s);
    lastMinute_ = t.tm_min;
  }
  lastWeatherOk_ = weatherGet().lastOkMs;
  lastSynced_ = synced;
  needRender_ = false;
}
