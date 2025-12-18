#include "Prefs.h"

const char *Prefs::PREF_NAMESPACE = "minitv";
const char *Prefs::PREF_SSID = "ssid";
const char *Prefs::PREF_PASS = "pass";
const char *Prefs::PREF_BRIGHTNESS = "brightness";
const char *Prefs::PREF_OSD_LEVEL = "osd_level";
const char *Prefs::PREF_TIMER_MINUTES = "timer_minutes";
const char *Prefs::PREF_SLIDESHOW_INTERVAL_SECONDS = "slideshow_sec";

Prefs::Prefs() {}

void Prefs::begin()
{
  if (!preferences.begin(PREF_NAMESPACE, false))
  {
    Serial.println("Failed to initialize preferences. Clearing and re-initializing.");
    preferences.clear();
    if (!preferences.begin(PREF_NAMESPACE, false))
    {
      Serial.println("Failed to initialize preferences even after clearing.");
    }
  }
}

String Prefs::getSsid()
{
  return readStringPreference(PREF_SSID);
}

void Prefs::setSsid(const String &ssid)
{
  writeStringPreference(PREF_SSID, ssid);
}

String Prefs::getPass()
{
  return readStringPreference(PREF_PASS);
}

void Prefs::setPass(const String &pass)
{
  writeStringPreference(PREF_PASS, pass);
}

int Prefs::getBrightness()
{
  return readIntPreference(PREF_BRIGHTNESS, 255); // Default brightness to 255
}

void Prefs::setBrightness(int brightness)
{
  writeIntPreference(PREF_BRIGHTNESS, brightness);
  if (brightness_changed_callback)
  {
    brightness_changed_callback(brightness);
  }
}

void Prefs::onBrightnessChanged(std::function<void(int)> callback)
{
  brightness_changed_callback = callback;
}

OSDLevel Prefs::getOsdLevel()
{
  return (OSDLevel)readIntPreference(PREF_OSD_LEVEL, 1); // Default to standard OSD level
}

void Prefs::setOsdLevel(int level)
{
  writeIntPreference(PREF_OSD_LEVEL, (int)level);
}

int Prefs::getTimerMinutes()
{
  return readIntPreference(PREF_TIMER_MINUTES, 0); // Default to 0 (disabled)
}

void Prefs::setTimerMinutes(int minutes)
{
  int clamped_minutes = constrain(minutes, 0, 60);
  writeIntPreference(PREF_TIMER_MINUTES, clamped_minutes);
  if (timer_minutes_changed_callback)
  {
    timer_minutes_changed_callback(clamped_minutes);
  }
}

void Prefs::onTimerMinutesChanged(std::function<void(int)> callback)
{
  timer_minutes_changed_callback = callback;
}

int Prefs::getSlideshowInterval()
{
  return readIntPreference(PREF_SLIDESHOW_INTERVAL_SECONDS, 5); // Default to 5
}

void Prefs::setSlideshowInterval(int seconds)
{
  int clamped_seconds = constrain(seconds, 1, 60);
  writeIntPreference(PREF_SLIDESHOW_INTERVAL_SECONDS, clamped_seconds);
  if (slideshow_interval_changed_callback)
  {
    slideshow_interval_changed_callback(clamped_seconds);
  }
}

void Prefs::onSlideshowIntervalChanged(std::function<void(int)> callback)
{
  slideshow_interval_changed_callback = callback;
}

String Prefs::readStringPreference(const char *key, const String &defaultValue)
{
  return preferences.getString(key, defaultValue);
}

void Prefs::writeStringPreference(const char *key, const String &value)
{
  preferences.putString(key, value);
}

int Prefs::readIntPreference(const char *key, int defaultValue)
{
  return preferences.getInt(key, defaultValue);
}

void Prefs::writeIntPreference(const char *key, int value)
{
  preferences.putInt(key, value);
}