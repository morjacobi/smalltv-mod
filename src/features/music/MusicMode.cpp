#include "MusicMode.h"
#include <Arduino_GFX_Library.h>
#include "Gfx.h"
#include "MusicClient.h"

MusicMode g_musicMode;

#define C_DIM   gfxTint(0xB574)   // secondary text
#define C_PANEL gfxTint(0x18E3)   // art placeholder fill

static const int ART = MUSIC_ART_PX;
static const int ART_X = (TFT_WIDTH - ART) / 2;
static const int ART_Y = 44;
static const int TEXT_TOP = ART_Y + ART + 12;     // start of the title/artist band

// A tiny quaver in the header, drawn from primitives (the 6x8 font has no glyph).
static void drawNote(Arduino_GFX* gfx, int x, int y, uint16_t c) {
  gfx->fillCircle(x, y + 12, 3, c);
  gfx->fillRect(x + 2, y, 2, 12, c);
  gfx->fillRect(x + 2, y, 6, 2, c);
}

// Pause bars, top-right, shown when the transport is paused.
static void drawPaused(Arduino_GFX* gfx, bool on) {
  uint16_t c = on ? C_DIM : C_BLACK;
  gfx->fillRect(220, 10, 3, 12, c);
  gfx->fillRect(226, 10, 3, 12, c);
}

static void drawTextBand(const MusicData& m) {
  Arduino_GFX* gfx = gfxDev();
  gfx->fillRect(0, TEXT_TOP, TFT_WIDTH, TFT_HEIGHT - TEXT_TOP, C_BLACK);

  uint8_t ts = gfxFitSize(m.title[0] ? m.title : "—", TFT_WIDTH - 16, 3);
  gfxDrawCentered(m.title[0] ? m.title : "—", TEXT_TOP + 4, ts, C_WHITE);

  if (m.artist[0]) {
    uint8_t as = gfxFitSize(m.artist, TFT_WIDTH - 16, 2);
    gfxDrawCentered(m.artist, TEXT_TOP + 4 + ts * GFX_FONT_H + 10, as, C_DIM);
  }
}

static void drawArt() {
  Arduino_GFX* gfx = gfxDev();
  if (musicArtReady()) {
    gfx->draw16bitRGBBitmap(ART_X, ART_Y, (uint16_t*)musicArt(), ART, ART);
  } else {
    gfx->fillRoundRect(ART_X, ART_Y, ART, ART, 10, C_PANEL);
    drawNote(gfx, ART_X + ART / 2 - 4, ART_Y + ART / 2 - 6, C_DIM);
  }
}

static void drawLayout(const MusicData& m) {
  Arduino_GFX* gfx = gfxDev();
  gfx->fillScreen(C_BLACK);
  drawNote(gfx, 10, 6, C_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(26, 8);
  gfx->print("MUSIC");
  drawPaused(gfx, !m.playing);
  drawArt();
  drawTextBand(m);
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

  bool textChanged = strncmp(m.title, lastTitle_, sizeof(lastTitle_)) != 0
                  || strncmp(m.artist, lastArtist_, sizeof(lastArtist_)) != 0;
  bool pauseChanged = m.playing != lastPlaying_;
  bool artChanged = m.artSeq != lastArtSeq_;

  if (!layoutPrimed_) {
    drawLayout(m);
    layoutPrimed_ = true;
    needRender_ = false;
  } else if (textChanged || pauseChanged || artChanged || needRender_) {
    if (artChanged) drawArt();
    if (textChanged) drawTextBand(m);
    if (pauseChanged) drawPaused(gfxDev(), !m.playing);
    needRender_ = false;
  } else {
    return;
  }

  strlcpy(lastTitle_, m.title, sizeof(lastTitle_));
  strlcpy(lastArtist_, m.artist, sizeof(lastArtist_));
  lastPlaying_ = m.playing;
  lastArtSeq_ = m.artSeq;
}
