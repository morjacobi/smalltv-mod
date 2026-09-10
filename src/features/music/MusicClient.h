// MusicClient.h — holds the pushed now-playing state and the album-art frame.
//
// Push-only, like the notify overlay: the Mac daemon POSTs {t,a,p} JSON to
// /api/music and streams a raw MUSIC_ART_PX^2 RGB565 (little-endian) frame to
// /api/music/art. Nothing here fetches.
#pragma once
#include <Arduino.h>
#include "config.h"
#include "MusicData.h"

// ---- metadata push ------------------------------------------------------------
bool musicApply(const String& body);      // parse {"t":..,"a":..,"p":bool}
const MusicData& musicGet();
bool musicFresh(uint32_t withinMs);

// ---- album-art stream (ESP8266WebServer upload callback) ---------------------
void musicArtBegin();
void musicArtWrite(const uint8_t* buf, size_t len);
void musicArtEnd();
void musicArtAbort();
bool musicArtOk();                         // last frame completed to exact size
bool musicArtReady();                      // a valid frame is in the buffer
const uint16_t* musicArt();                // MUSIC_ART_PX * MUSIC_ART_PX pixels
