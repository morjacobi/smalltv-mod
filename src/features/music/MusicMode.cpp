#include "MusicMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "MusicClient.h"

MusicMode g_musicMode;

#define C_DIM gfxTint(0xC618)   // light grey for the artist line

static const int ART = MUSIC_ART_PX;   // source frame edge (kept resident, 8 KB at 64)

// A tiny quaver in the header, drawn from primitives (the 6x8 font has no glyph).
static void drawNote(Arduino_GFX* gfx, int x, int y, uint16_t c) {
  gfx->fillCircle(x, y + 12, 3, c);
  gfx->fillRect(x + 2, y, 2, 12, c);
  gfx->fillRect(x + 2, y, 6, 2, c);
}

// Pause bars, top-right, shown when the transport is paused.
static void drawPaused(Arduino_GFX* gfx) {
  gfx->fillRect(219, 9, 4, 14, C_BLACK);
  gfx->fillRect(226, 9, 4, 14, C_BLACK);
  gfx->fillRect(220, 10, 3, 12, C_WHITE);
  gfx->fillRect(227, 10, 3, 12, C_WHITE);
}

// The resident ART*ART frame, nearest-neighbour upscaled to fill 240x240 and
// dimmed to ~44% so overlaid white text stays legible. One 240px line buffer.
static void drawArtBg() {
  Arduino_GFX* gfx = gfxDev();
  if (!musicArtReady()) {
    gfx->fillScreen(C_BLACK);
    drawNote(gfx, TFT_WIDTH / 2 - 6, TFT_HEIGHT / 2 - 20, C_DIM);
    return;
  }
  const uint16_t* src = musicArt();
  uint16_t line[TFT_WIDTH];
  for (int y = 0; y < TFT_HEIGHT; y++) {
    const uint16_t* srow = src + (y * ART / TFT_HEIGHT) * ART;
    for (int x = 0; x < TFT_WIDTH; x++) {
      uint16_t px = srow[x * ART / TFT_WIDTH];
      uint16_t r = (px >> 11) & 0x1F, g = (px >> 5) & 0x3F, b = px & 0x1F;
      line[x] = (((r * 7) >> 4) << 11) | (((g * 7) >> 4) << 5) | ((b * 7) >> 4);
    }
    gfx->draw16bitRGBBitmap(0, y, line, TFT_WIDTH, 1);
  }
}

// Centred text with a 2px black drop shadow — readable over any album art.
static void shadowText(const char* s, int y, uint8_t size, uint16_t col) {
  Arduino_GFX* gfx = gfxDev();
  int x = (TFT_WIDTH - gfxTextW(s, size)) / 2;
  if (x < 2) x = 2;
  gfx->setTextSize(size);
  gfx->setTextColor(C_BLACK);
  gfx->setCursor(x + 2, y + 2);
  gfx->print(s);
  gfx->setTextColor(col);
  gfx->setCursor(x, y);
  gfx->print(s);
}

static void drawAll(const MusicData& m) {
  Arduino_GFX* gfx = gfxDev();
  drawArtBg();

  drawNote(gfx, 11, 7, C_BLACK);
  drawNote(gfx, 10, 6, C_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(C_BLACK); gfx->setCursor(28, 10); gfx->print("MUSIC");
  gfx->setTextColor(C_WHITE); gfx->setCursor(26, 8);  gfx->print("MUSIC");
  if (!m.playing) drawPaused(gfx);

  const char* title = m.title[0] ? m.title : "(nothing playing)";
  uint8_t ts = gfxFitSize(title, TFT_WIDTH - 16, 3);
  int ty = 150;
  shadowText(title, ty, ts, C_WHITE);
  if (m.artist[0]) {
    uint8_t as = gfxFitSize(m.artist, TFT_WIDTH - 16, 2);
    shadowText(m.artist, ty + ts * GFX_FONT_H + 12, as, C_DIM);
  }
}

// ---- DisplayMode --------------------------------------------------------------
void MusicMode::begin(const Settings&) {
  needRender_ = true;
  layoutPrimed_ = false;
  lastTitle_[0] = lastArtist_[0] = 0;
  lastArtSeq_ = 0xFFFF;
}

void MusicMode::invalidate(const Settings&) {
  needRender_ = true;
  layoutPrimed_ = false;
}

void MusicMode::service(const Settings&) {
  const MusicData& m = musicGet();

  bool changed = !layoutPrimed_ || needRender_
              || m.artSeq != lastArtSeq_
              || m.playing != lastPlaying_
              || strncmp(m.title, lastTitle_, sizeof(lastTitle_)) != 0
              || strncmp(m.artist, lastArtist_, sizeof(lastArtist_)) != 0;
  if (!changed) return;

  drawAll(m);
  layoutPrimed_ = true;
  needRender_ = false;
  strlcpy(lastTitle_, m.title, sizeof(lastTitle_));
  strlcpy(lastArtist_, m.artist, sizeof(lastArtist_));
  lastPlaying_ = m.playing;
  lastArtSeq_ = m.artSeq;
}
