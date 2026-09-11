#include "ClockMode.h"
#include <Arduino_GFX_Library.h>
#include <math.h>
#include "Gfx.h"
#include "Clock.h"
#include "WeatherClient.h"

ClockMode g_clockMode;

// House palette (same formula as UsageMode: gfxTint() picks up the Display
// tab's per-panel colour correction).
#define C_ACCENT gfxTint(0xDBAA)   // terra-cotta 0xd97757 — temp, accents
#define C_SKY    gfxTint(0x469F)   // sky blue — sun/cloud icons
#define C_UGREEN gfxTint(0x7C6B)   // green 0x788c5d — date line
#define C_DIM    gfxTint(0xB574)   // secondary text

// The blinking colon: geometry recorded by the last full draw so the
// once-a-second toggle can redraw just that one glyph cell (no flicker, no
// full-screen clear — the panel has no framebuffer to double-buffer with).
static int  s_timeX = -1, s_timeY = -1;
static bool s_colonOn = true;
static int  s_lastSecond = -1;

static const char* kWeekday[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                  "Thursday", "Friday", "Saturday"};
static const char* kMonth[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// ---- small vector icons (no bitmap font glyphs for any of this) -----------
static void drawSun(Arduino_GFX* gfx, int cx, int cy, uint16_t c) {
  gfx->fillCircle(cx, cy, 7, c);
  for (int a = 0; a < 8; a++) {
    float rad = a * PI / 4;
    int x0 = cx + (int)(cosf(rad) * 10), y0 = cy + (int)(sinf(rad) * 10);
    int x1 = cx + (int)(cosf(rad) * 14), y1 = cy + (int)(sinf(rad) * 14);
    gfx->drawLine(x0, y0, x1, y1, c);
  }
}

static void drawCloud(Arduino_GFX* gfx, int cx, int cy, uint16_t c) {
  gfx->fillCircle(cx - 7, cy + 2, 7, c);
  gfx->fillCircle(cx + 5, cy + 2, 9, c);
  gfx->fillCircle(cx - 1, cy - 4, 8, c);
  gfx->fillRoundRect(cx - 13, cy, 28, 9, 4, c);
}

static void drawRain(Arduino_GFX* gfx, int cx, int cy, uint16_t c) {
  drawCloud(gfx, cx, cy - 4, c);
  for (int i = -1; i <= 1; i++)
    gfx->drawLine(cx + i * 8, cy + 10, cx + i * 8 - 3, cy + 17, c);
}

static void drawSnow(Arduino_GFX* gfx, int cx, int cy, uint16_t c) {
  drawCloud(gfx, cx, cy - 4, c);
  for (int i = -1; i <= 1; i++) gfx->fillCircle(cx + i * 8, cy + 14, 2, c);
}

static void drawStorm(Arduino_GFX* gfx, int cx, int cy, uint16_t c) {
  drawCloud(gfx, cx, cy - 4, c);
  int bx = cx - 2, by = cy + 8;
  gfx->fillTriangle(bx + 6, by, bx - 2, by + 9, bx + 3, by + 9, c);
  gfx->fillTriangle(bx + 3, by + 9, bx - 3, by + 18, bx + 5, by + 9, c);
}

static void drawFog(Arduino_GFX* gfx, int cx, int cy, uint16_t c) {
  for (int i = 0; i < 4; i++)
    gfx->fillRoundRect(cx - 14, cy - 6 + i * 6, 28, 3, 1, c);
}

static void drawWeatherIcon(Arduino_GFX* gfx, int cx, int cy, WeatherIcon icon, uint16_t c) {
  switch (icon) {
    case WICON_SUN:   drawSun(gfx, cx, cy, c); break;
    case WICON_RAIN:  drawRain(gfx, cx, cy, c); break;
    case WICON_SNOW:  drawSnow(gfx, cx, cy, c); break;
    case WICON_STORM: drawStorm(gfx, cx, cy, c); break;
    case WICON_FOG:   drawFog(gfx, cx, cy, c); break;
    default:          drawCloud(gfx, cx, cy, c); break;
  }
}

// Thermometer: a tube with a filled bulb.
static void drawThermo(Arduino_GFX* gfx, int x, int y, uint16_t c) {
  gfx->fillRoundRect(x + 3, y, 6, 18, 3, c);
  gfx->fillCircle(x + 6, y + 21, 7, c);
  gfx->fillCircle(x + 6, y + 21, 4, C_BLACK);
  gfx->fillRoundRect(x + 4, y + 6, 4, 16, 2, C_BLACK);
  gfx->fillRoundRect(x + 4, y + 12, 4, 10, 2, c);
}

// Droplet: a circle with a pointed top.
static void drawDroplet(Arduino_GFX* gfx, int x, int y, uint16_t c) {
  gfx->fillCircle(x + 7, y + 14, 8, c);
  gfx->fillTriangle(x + 7, y, x - 1, y + 12, x + 15, y + 12, c);
}

// ---- layout ----------------------------------------------------------------
static void drawSynced(struct tm& t, const Settings& s) {
  Arduino_GFX* gfx = gfxDev();
  gfx->fillScreen(C_BLACK);

  const WeatherData& w = weatherGet();
  const char* label = s.weather.label.length() ? s.weather.label.c_str() : nullptr;

  // Header: location label (left) / weather icon (right).
  if (label) {
    gfx->setTextSize(2);
    gfx->setTextColor(C_ACCENT);
    gfx->setCursor(10, 8);
    gfx->print(label);
  }
  if (w.valid) drawWeatherIcon(gfx, TFT_WIDTH - 26, 20, weatherCodeToIcon(w.code), C_SKY);

  // Big time. The colon is drawn as part of the string here (full repaint);
  // service() re-flashes just that glyph cell every second afterwards.
  char hm[8];
  snprintf(hm, sizeof(hm), "%02d:%02d", t.tm_hour, t.tm_min);
  gfx->setTextSize(6);
  gfx->setTextColor(C_WHITE);
  int hw = gfxTextW(hm, 6);
  s_timeX = (TFT_WIDTH - hw) / 2;
  s_timeY = 46;
  gfx->setCursor(s_timeX, s_timeY);
  gfx->print(hm);
  s_colonOn = true;
  s_lastSecond = t.tm_sec;

  // Weekday, date.
  char date[28];
  snprintf(date, sizeof(date), "%s  %s %d", kWeekday[t.tm_wday], kMonth[t.tm_mon], t.tm_mday);
  gfxDrawCentered(date, 104, 2, C_UGREEN);

  gfx->drawFastHLine(24, 130, TFT_WIDTH - 48, C_DGRAY);

  if (!w.valid) {
    gfxDrawCentered(w.error ? "weather error" : "weather...", 152, 2, C_DIM);
    return;
  }

  // Temp row: thermometer + big value + condition, high/low on the right.
  drawThermo(gfx, 18, 142, C_ACCENT);
  char t1[8];
  snprintf(t1, sizeof(t1), "%d%s", w.temp, s.weather.fahrenheit ? "F" : "C");
  gfx->setTextSize(3);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(46, 146);
  gfx->print(t1);
  gfx->setTextSize(2);
  gfx->setTextColor(C_DIM);
  int cw = gfxTextW(w.condition, 2);
  gfx->setCursor(TFT_WIDTH - cw - 14, 152);
  gfx->print(w.condition);

  // Humidity row: droplet + value, high/low alongside.
  drawDroplet(gfx, 18, 178, C_SKY);
  char hline[10];
  snprintf(hline, sizeof(hline), "%d%%", w.humidity);
  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(46, 182);
  gfx->print(hline);

  char hl[20];
  snprintf(hl, sizeof(hl), "H:%d  L:%d", w.high, w.low);
  int hlw = gfxTextW(hl, 2);
  gfx->setTextSize(2);
  gfx->setTextColor(C_DIM);
  gfx->setCursor(TFT_WIDTH - hlw - 14, 182);
  gfx->print(hl);
}

// Flip the colon on/off by redrawing just its character cell — one glyph,
// not the screen. GFX_FONT_W*size is one monospace cell; the colon is the
// 3rd character of "HH:MM".
static void tickColon(bool on) {
  if (s_timeX < 0) return;
  Arduino_GFX* gfx = gfxDev();
  int cellW = GFX_FONT_W * 6, x = s_timeX + 2 * cellW;
  gfx->fillRect(x, s_timeY, cellW, GFX_FONT_H * 6, C_BLACK);
  if (on) {
    gfx->setTextSize(6);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(x, s_timeY);
    gfx->print(':');
  }
  s_colonOn = on;
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

  if (!needRender_ && !minuteChanged && !weatherChanged && !syncChanged) {
    // No full repaint due — just the once-a-second colon flash.
    if (synced && t.tm_sec != s_lastSecond) {
      s_lastSecond = t.tm_sec;
      tickColon(!s_colonOn);
    }
    return;
  }

  Arduino_GFX* gfx = gfxDev();
  if (!synced) {
    gfx->fillScreen(C_BLACK);
    gfxDrawCentered("syncing clock...", 116, 2, C_DIM);
    s_timeX = -1;   // no colon to blink while unsynced
  } else {
    drawSynced(t, s);
    lastMinute_ = t.tm_min;
  }
  lastWeatherOk_ = weatherGet().lastOkMs;
  lastSynced_ = synced;
  needRender_ = false;
}
