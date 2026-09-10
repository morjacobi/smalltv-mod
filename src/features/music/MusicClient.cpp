#include "MusicClient.h"
#include <ArduinoJson.h>

// The one big allocation this feature costs: MUSIC_ART_PX^2 * 2 bytes of BSS
// (96x96 -> 18,432 B). Kept resident so a carousel wake() can repaint the art
// without the daemon re-pushing it.
static uint16_t g_art[MUSIC_ART_PX * MUSIC_ART_PX];
static const size_t ART_BYTES = sizeof(g_art);

static MusicData g_music;
static size_t    g_artPos = 0;      // byte cursor while a frame streams in
static bool      g_artFilling = false;
static bool      g_artOk = false;   // last stream finished at exactly ART_BYTES
static bool      g_artReady = false;

// ---- metadata ---------------------------------------------------------------
// Contract: { "t":"title", "a":"artist", "p":true }  — p defaults true if absent.
bool musicApply(const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  if (!doc["t"].is<const char*>()) return false;   // title is required

  strlcpy(g_music.title,  doc["t"] | "", sizeof(g_music.title));
  strlcpy(g_music.artist, doc["a"] | "", sizeof(g_music.artist));
  g_music.playing = doc["p"] | true;
  g_music.valid   = true;
  g_music.lastMs  = millis();
  return true;
}

const MusicData& musicGet() { return g_music; }

bool musicFresh(uint32_t withinMs) {
  return g_music.valid && (millis() - g_music.lastMs) <= withinMs;
}

// ---- album-art stream -----------------------------------------------------
void musicArtBegin() {
  g_artPos = 0;
  g_artFilling = true;
  g_artOk = false;
}

void musicArtWrite(const uint8_t* buf, size_t len) {
  if (!g_artFilling) return;
  if (g_artPos + len > ART_BYTES) len = ART_BYTES - g_artPos;   // clamp, never overrun
  memcpy((uint8_t*)g_art + g_artPos, buf, len);
  g_artPos += len;
}

void musicArtEnd() {
  g_artFilling = false;
  g_artOk = (g_artPos == ART_BYTES);
  if (g_artOk) {
    g_artReady = true;
    g_music.artSeq++;
  }
}

void musicArtAbort() {
  g_artFilling = false;
  g_artOk = false;
}

bool musicArtOk()    { return g_artOk; }
bool musicArtReady() { return g_artReady; }
const uint16_t* musicArt() { return g_art; }
