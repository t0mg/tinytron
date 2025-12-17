#pragma once

#include "JPEGDEC.h"
#include <Arduino.h>
#include <list>
#include <string>

#include "OSD.h"

class Display;
class Prefs;
class Battery;

struct TimedOsd
{
  std::string text;
  OSDPosition position;
  OSDLevel level;
  uint32_t endTime;
};

enum class MediaPlayerState
{
  STOPPED,
  PLAYING,
  PAUSED,
  STATIC
};

int _doDraw(JPEGDRAW *pDraw);

class MediaPlayer
{
protected:
  Display &mDisplay;
  Prefs &mPrefs;
  Battery &mBattery;
  JPEGDEC mJpeg;

  MediaPlayerState mState = MediaPlayerState::STOPPED;

  TaskHandle_t mTaskHandle = NULL;
  volatile bool mRunTask = false;

  std::list<TimedOsd> mTimedOsds;

  uint8_t *mCurrentFrame = NULL;
  size_t mCurrentFrameSize = 0;

  SemaphoreHandle_t mMutex = NULL;

  static void _task(void *param);
  void task();
  void startTask();

  virtual bool getFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength) = 0;
  virtual void onFrameDisplayed() {};
  virtual void onStateChanged(MediaPlayerState oldState, MediaPlayerState newState) {};
  virtual void onLoop() {};
  virtual void onStatic() {};

  friend int _doDraw(JPEGDRAW *pDraw);

public:
  MediaPlayer(Display &display, Prefs &prefs, Battery &battery);
  virtual ~MediaPlayer();

  virtual void start();
  void play();
  void stop();
  void pause();
  void playPauseToggle();
  virtual void next() {}
  virtual void set(int index) {}

  void drawOSDTimed(const std::string &text, OSDPosition position,
                    OSDLevel level, uint32_t durationMs = 2000);

  MediaPlayerState getState() { return mState; }
};
