#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "OSD.h"
#include <functional>

class Prefs
{
public:
  Prefs();
  void begin();

  String getSsid();
  void setSsid(const String &ssid);

  String getPass();
  void setPass(const String &pass);

  int getBrightness();
  void setBrightness(int brightness);

  OSDLevel getOsdLevel();
  void setOsdLevel(int level);

  int getTimerMinutes();
  void setTimerMinutes(int minutes);

  int getSlideshowInterval();
  void setSlideshowInterval(int seconds);

  void onBrightnessChanged(std::function<void(int)> callback);
  void onTimerMinutesChanged(std::function<void(int)> callback);
  void onSlideshowIntervalChanged(std::function<void(int)> callback);

private:
  Preferences preferences;
  std::function<void(int)> brightness_changed_callback;
  std::function<void(int)> timer_minutes_changed_callback;
  std::function<void(int)> slideshow_interval_changed_callback;

  static const char *PREF_NAMESPACE;
  static const char *PREF_SSID;
  static const char *PREF_PASS;
  static const char *PREF_BRIGHTNESS;
  static const char *PREF_OSD_LEVEL;
  static const char *PREF_TIMER_MINUTES;
  static const char *PREF_SLIDESHOW_INTERVAL_SECONDS;

  String readStringPreference(const char *key, const String &defaultValue = "");
  void writeStringPreference(const char *key, const String &value);
  int readIntPreference(const char *key, int defaultValue);
  void writeIntPreference(const char *key, int value);
};