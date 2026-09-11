// ClockMode.h — time, date and weather. Time comes from Clock.h (already
// running for night mode); weather from WeatherClient (plain HTTP, no TLS).
#pragma once
#include "Mode.h"
#include "config.h"

class ClockMode : public DisplayMode {
 public:
  const char* id() const override { return "clock"; }
  uint8_t     modeConst() const override { return MODE_CLOCK; }

  void begin(const Settings& s) override;
  void service(const Settings& s) override;
  void invalidate(const Settings& s) override;
  void wake(const Settings& s) override { needRender_ = true; }

 private:
  bool needRender_ = true;
  int  lastMinute_ = -1;
  uint32_t lastWeatherOk_ = 0xFFFFFFFF;
  bool lastSynced_ = false;
};

extern ClockMode g_clockMode;
