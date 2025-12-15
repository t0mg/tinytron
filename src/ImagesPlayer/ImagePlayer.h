#pragma once

#include "JPEGDEC.h"
#include <Arduino.h>
#include <list>
#include <string>

#include "../OSD.h"

class Display;
class Prefs;
class Battery;
class ImageSource;

struct TimedOsdImage {
  std::string text;
  OSDPosition position;
  OSDLevel level;
  uint32_t endTime;
};

enum class ImagePlayerState { STOPPED, PLAYING, PAUSED };

class ImagePlayer {
private:
  Display &mDisplay;
  Prefs &mPrefs;
  Battery &mBattery;
  ImageSource *mImageSource = NULL;

  JPEGDEC mJpeg;
  ImagePlayerState mState = ImagePlayerState::STOPPED;

  TaskHandle_t mFrameTaskHandle = NULL;
  volatile bool mRunTask = false;

  std::list<TimedOsdImage> mTimedOsds;

  uint8_t *mCurrentFrame = NULL;
  size_t mCurrentFrameSize = 0;

  uint32_t mLastAdvanceMs = 0;
  SemaphoreHandle_t mMutex = NULL;

  static void _frameTask(void *param);
  void frameTask();
  void startTask();

  friend int _doDrawImage(JPEGDRAW *pDraw);

public:
  ImagePlayer(ImageSource *imageSource, Display &display, Prefs &prefs,
              Battery &battery);

  void start();
  void play();
  void stop();
  void pause();
  void playPauseToggle();

  void setImage(int index);
  void nextImage();

  void drawOSDTimed(const std::string &text, OSDPosition position,
                    OSDLevel level, uint32_t durationMs = 2000);

  ImagePlayerState getState() { return mState; }
};
