#include "VideoPlayer.h"
#include "Display.h"
#include "Prefs.h"
#include "VideoSource.h"
#include "Battery.h"
#include <Arduino.h>
#include <list>

VideoPlayer::VideoPlayer(VideoSource *videoSource, Display &display,
                         Prefs &prefs, Battery &battery)
    : MediaPlayer(display, prefs, battery),
      mVideoSource(videoSource)
{
}

void VideoPlayer::start()
{
  mVideoSource->start();
  MediaPlayer::start();
}

void VideoPlayer::set(int channel)
{
  if (mTaskHandle != NULL)
  {
    mRunTask = false;
    // wait for the task to stop
    while (mTaskHandle != NULL)
    {
      vTaskDelay(10);
    }
  }
  // update the video source
  mVideoSource->setChannel(channel);
  drawOSDTimed(mVideoSource->getChannelName(), TOP_LEFT, OSDLevel::STANDARD);
  startTask();
}

void VideoPlayer::next()
{
  if (mState == MediaPlayerState::PAUSED)
  {
    play();
  }

  if (mTaskHandle != NULL)
  {
    mRunTask = false;
    // wait for the task to stop
    while (mTaskHandle != NULL)
    {
      vTaskDelay(10);
    }
  }
  mVideoSource->nextChannel();
  drawOSDTimed(mVideoSource->getChannelName(), TOP_LEFT, OSDLevel::STANDARD);
  startTask();
}

void VideoPlayer::playStatic()
{
  if (mState == MediaPlayerState::STATIC)
  {
    return;
  }

  // If a task is running, stop it cleanly
  if (mTaskHandle != NULL)
  {
    mRunTask = false;
    while (mTaskHandle != NULL)
    {
      vTaskDelay(10);
    }
  }

  auto oldState = mState;
  mState = MediaPlayerState::STATIC;
  onStateChanged(oldState, mState);

  mDisplay.fillScreen(DisplayColors::BLACK);

  // Start the task in STATIC mode
  startTask();
}

void VideoPlayer::redrawFrame()
{
  if (mCurrentFrame)
  {
    if (mJpeg.openRAM(mCurrentFrame, mCurrentFrameSize, _doDraw))
    {
      mJpeg.setUserPointer(this);
      mJpeg.setPixelType(RGB565_BIG_ENDIAN);
      mJpeg.decode(0, 0, 0);
      mJpeg.close();
    }
    mDisplay.flushSprite();
  }
  else
  {
    mDisplay.fillScreen(DisplayColors::BLACK);
  }
}

bool VideoPlayer::getFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength)
{
  if (!mVideoSource)
  {
    return false;
  }
  return mVideoSource->getVideoFrame(buffer, bufferLength, frameLength);
}

void VideoPlayer::onStateChanged(MediaPlayerState oldState, MediaPlayerState newState)
{
  mVideoSource->setState(newState);
}

static unsigned short x = 12345, y = 6789, z = 42, w = 1729;

unsigned short xorshift16()
{
  unsigned short t = x ^ (x << 5);
  x = y;
  y = z;
  z = w;
  w = w ^ (w >> 1) ^ t ^ (t >> 3);
  return w & 0xFFFF;
}

void VideoPlayer::onStatic()
{
  // draw random pixels to the screen to simulate static
  // we'll do this 8 rows of pixels at a time to save RAM
  int width = mDisplay.width();
  int height = 8;
  uint16_t *staticBuffer = (uint16_t *)malloc(width * height * 2);
  for (int i = 0; i < mDisplay.height(); i++)
  {
    if (!mRunTask)
    {
      break;
    }
    for (int p = 0; p < width * height; p++)
    {
      int grey = xorshift16() >> 8;
      staticBuffer[p] = mDisplay.color565(grey, grey, grey);
    }
    mDisplay.drawPixels(0, i * height, width, height, staticBuffer);
  }
  free(staticBuffer);
}

void VideoPlayer::onFrameDisplayed()
{
  OSDLevel osdLevel = mPrefs.getOsdLevel();
  if (osdLevel >= OSDLevel::DEBUG)
  {
    mFrameTimes.push_back(millis());
    // keep the frame rate elapsed time to 5 seconds
    while (mFrameTimes.size() > 0 &&
           mFrameTimes.back() - mFrameTimes.front() > 5000)
    {
      mFrameTimes.pop_front();
    }
  }
  if (osdLevel >= OSDLevel::DEBUG)
  {
    char fpsText[8];
    sprintf(fpsText, "%d FPS", mFrameTimes.size() / 5);
    mDisplay.drawOSD(fpsText, BOTTOM_RIGHT, OSDLevel::DEBUG);
    char batText[16];
    sprintf(batText, "%d%% %.2f", mBattery.getBatteryLevel(),
            mBattery.getVoltage());
    mDisplay.drawOSD(batText, BOTTOM_LEFT, OSDLevel::DEBUG);
  }
}