// MusicData.h — runtime (volatile) now-playing snapshot pushed by the daemon.
#pragma once
#include <Arduino.h>

struct MusicData {
  char     title[64];
  char     artist[64];
  bool     playing;      // transport state at last push
  bool     valid;        // got at least one push
  uint32_t lastMs;       // millis() of last push
  uint16_t artSeq;       // bumped each time a full art frame lands

  void clear() {
    title[0] = artist[0] = 0;
    playing = false;
    valid = false;
    lastMs = 0;
    artSeq = 0;
  }
};
