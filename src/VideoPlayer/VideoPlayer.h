#pragma once
#include <Arduino.h>
#include "JPEGDEC.h"

#include "VideoPlayerState.h"
#include "StreamVideoSource.h"

#include "Battery.h"
#include "Prefs.h"
#include "OSD.h"

class Display;
class Prefs;
class VideoSource;

struct TimedOsd
{
  std::string text;
  OSDPosition position;
  OSDLevel level;
  uint32_t endTime;
};

class VideoPlayer {
  private:
    int mChannelVisible = 0;
    VideoPlayerState mState = VideoPlayerState::STOPPED;

    // video playing
    Display &mDisplay;
    JPEGDEC mJpeg;
    Prefs &mPrefs;
    Battery &mBattery;

    // video source
    VideoSource *mVideoSource = NULL;

    TaskHandle_t _framePlayerTaskHandle = NULL;
    volatile bool m_runTask = false;

    static void _framePlayerTask(void *param);

    void framePlayerTask();
    void playTask();
    void drawOSD(int fps);

    // timed OSD
    std::list<TimedOsd> _timedOsds;

    // current frame data - for redraw
    uint8_t *_currentFrame = NULL;
    size_t _currentFrameSize = 0;

    friend int _doDraw(JPEGDRAW *pDraw);

  public:
    VideoPlayer(VideoSource *videoSource, Display &display, Prefs &prefs, Battery &battery);
    void setChannel(int channelIndex);
    void nextChannel();
    void start();
    void play();
    void stop();
    void pause();
    void playStatic();
    void playPauseToggle();
    void drawOSDTimed(const std::string& text, OSDPosition position, OSDLevel level, uint32_t durationMs = 2000);
    void redrawFrame();
    VideoPlayerState getState() { return mState; }
};