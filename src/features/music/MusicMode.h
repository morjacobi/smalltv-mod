// MusicMode.h — "now playing" screen fed by the media-control daemon.
//
// Album art + title + artist, laid out like a phone lock-screen card. Push-only
// (MusicClient); this mode just renders and dirty-tracks. When the daemon goes
// quiet or playback stops, carouselHas() in main.cpp drops it from the rotation.
#pragma once
#include "Mode.h"
#include "config.h"
#include "MusicData.h"

class MusicMode : public DisplayMode {
 public:
  const char* id() const override { return "music"; }
  uint8_t     modeConst() const override { return MODE_MUSIC; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override;
  void wake(const Settings& s) override { needRender_ = true; layoutPrimed_ = false; }

 private:
  bool     needRender_ = true;
  bool     layoutPrimed_ = false;
  char     lastTitle_[64] = {0};
  char     lastArtist_[64] = {0};
  bool     lastPlaying_ = false;
  uint16_t lastArtSeq_ = 0xFFFF;
};

extern MusicMode g_musicMode;
