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

// The animated weather icon: same idea as the colon — redraw only its own
// box every tick, not the screen. Center is fixed by the layout below.
static const int  ICON_CX = TFT_WIDTH - 26, ICON_CY = 20;
static const int  ICON_X0 = ICON_CX - 22, ICON_Y0 = 0, ICON_W = 44, ICON_H = 46;
static const uint16_t ICON_FRAME_MS = 120;   // ~8 fps
static uint8_t     s_iconAnimOn = 0;         // whether an icon is currently drawn at all
static uint8_t     s_iconFrame = 0;
static uint32_t    s_iconNextMs = 0;

static const char* kWeekday[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                  "Thursday", "Friday", "Saturday"};
static const char* kMonth[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// ---- small vector icons, animated by `phase` (0..1, one loop) -------------
// No bitmap assets: each frame is just the same primitives at moved
// coordinates, redrawn into ICON_X0/Y0/W/H — cheap enough to do every tick.
static void drawSun(Arduino_GFX* gfx, int cx, int cy, uint16_t c, float phase) {
  gfx->fillCircle(cx, cy, 7, c);
  float spin = phase * 2 * PI;
  for (int a = 0; a < 8; a++) {
    float rad = a * PI / 4 + spin;
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

// Drift for the plain-cloud icon — needs a visible amplitude or "no motion"
// reads as "frozen", not calm. Side-to-side plus a slight rise/fall so it
// doesn't read as a simple side-to-side jitter.
static void drawCloudDrift(Arduino_GFX* gfx, int cx, int cy, uint16_t c, float phase) {
  float a = phase * 2 * PI;
  drawCloud(gfx, cx + (int)(sinf(a) * 7), cy + (int)(cosf(a) * 2), c);
}

static void drawRain(Arduino_GFX* gfx, int cx, int cy, uint16_t c, float phase) {
  drawCloud(gfx, cx, cy - 4, c);
  for (int i = -1; i <= 1; i++) {
    float p = fmodf(phase * 2 + i * 0.33f, 1.0f);   // 0 (at cloud) .. 1 (below)
    int y0 = cy + 9 + (int)(p * 9), y1 = y0 + 6;
    gfx->drawLine(cx + i * 8, y0, cx + i * 8 - 2, y1, c);
  }
}

static void drawSnow(Arduino_GFX* gfx, int cx, int cy, uint16_t c, float phase) {
  drawCloud(gfx, cx, cy - 4, c);
  for (int i = -1; i <= 1; i++) {
    float p = fmodf(phase * 1.2f + i * 0.33f, 1.0f);
    int x = cx + i * 8 + (int)(sinf(phase * 2 * PI + i) * 2);
    int y = cy + 10 + (int)(p * 11);
    gfx->fillCircle(x, y, 2, c);
  }
}

static void drawStorm(Arduino_GFX* gfx, int cx, int cy, uint16_t c, float phase) {
  drawCloud(gfx, cx, cy - 4, c);
  bool flash = phase < 0.12f || (phase > 0.45f && phase < 0.55f);
  if (!flash) return;
  int bx = cx - 2, by = cy + 8;
  gfx->fillTriangle(bx + 6, by, bx - 2, by + 9, bx + 3, by + 9, c);
  gfx->fillTriangle(bx + 3, by + 9, bx - 3, by + 18, bx + 5, by + 9, c);
}

static void drawFog(Arduino_GFX* gfx, int cx, int cy, uint16_t c, float phase) {
  for (int i = 0; i < 4; i++) {
    int dx = (int)(sinf(phase * 2 * PI + i * 1.2f) * 4);
    gfx->fillRoundRect(cx - 14 + dx, cy - 6 + i * 6, 28, 3, 1, c);
  }
}

static void drawWeatherIcon(Arduino_GFX* gfx, int cx, int cy, WeatherIcon icon, uint16_t c,
                            float phase) {
  switch (icon) {
    case WICON_SUN:   drawSun(gfx, cx, cy, c, phase); break;
    case WICON_RAIN:  drawRain(gfx, cx, cy, c, phase); break;
    case WICON_SNOW:  drawSnow(gfx, cx, cy, c, phase); break;
    case WICON_STORM: drawStorm(gfx, cx, cy, c, phase); break;
    case WICON_FOG:   drawFog(gfx, cx, cy, c, phase); break;
    default:          drawCloudDrift(gfx, cx, cy, c, phase); break;
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
  s_iconAnimOn = w.valid;
  if (w.valid) drawWeatherIcon(gfx, ICON_CX, ICON_CY, weatherCodeToIcon(w.code), C_SKY, 0.0f);

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

  // Temp row: thermometer + big value, own line.
  drawThermo(gfx, 18, 140, C_ACCENT);
  char t1[8];
  snprintf(t1, sizeof(t1), "%d%s", w.temp, s.weather.fahrenheit ? "F" : "C");
  gfx->setTextSize(3);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(46, 144);
  gfx->print(t1);

  // Condition, its own centred line below — long labels ("Partly cloudy")
  // collided with the temperature when they shared a row.
  gfxDrawCentered(w.condition, 176, 2, C_DIM);

  // Humidity row: droplet + value, high/low alongside.
  drawDroplet(gfx, 18, 200, C_SKY);
  char hline[10];
  snprintf(hline, sizeof(hline), "%d%%", w.humidity);
  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(46, 204);
  gfx->print(hline);

  char hl[20];
  snprintf(hl, sizeof(hl), "H:%d  L:%d", w.high, w.low);
  int hlw = gfxTextW(hl, 2);
  gfx->setTextSize(2);
  gfx->setTextColor(C_DIM);
  gfx->setCursor(TFT_WIDTH - hlw - 14, 204);
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

// One animation frame: erase the icon's box, redraw at the new phase. Only
// that ~44x46 box, never the screen.
static void tickIcon(const Settings& s) {
  if (!s_iconAnimOn) return;
  const WeatherData& w = weatherGet();
  if (!w.valid) { s_iconAnimOn = false; return; }
  Arduino_GFX* gfx = gfxDev();
  gfx->fillRect(ICON_X0, ICON_Y0, ICON_W, ICON_H, C_BLACK);
  s_iconFrame = (s_iconFrame + 1) % 24;   // 24 frames @120ms = ~2.9s/cycle: fast enough to read as motion
  drawWeatherIcon(gfx, ICON_CX, ICON_CY, weatherCodeToIcon(w.code), C_SKY,
                 s_iconFrame / 24.0f);
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
    // No full repaint due — just the once-a-second colon flash and the
    // weather icon's own animation, both partial redraws.
    if (synced && t.tm_sec != s_lastSecond) {
      s_lastSecond = t.tm_sec;
      tickColon(!s_colonOn);
    }
    if (synced && (int32_t)(millis() - s_iconNextMs) >= 0) {
      s_iconNextMs = millis() + ICON_FRAME_MS;
      tickIcon(s);
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
