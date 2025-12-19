#include "MediaPlayer.h"
#include "Display.h"
#include "Prefs.h"
#include "Battery.h"

int _doDraw(JPEGDRAW *pDraw)
{
  MediaPlayer *player = (MediaPlayer *)pDraw->pUser;
  // calculate the x offset to center the image
  int x_offset = 0;
  int imageWidth = player->mJpeg.getWidth();
  int screenWidth = player->mDisplay.width();
  x_offset = (screenWidth - imageWidth) / 2;
  player->mDisplay.drawPixelsToSprite(pDraw->x + x_offset, pDraw->y,
                                      pDraw->iWidth, pDraw->iHeight,
                                      pDraw->pPixels);
  return 1;
}

void MediaPlayer::_task(void *param)
{
  MediaPlayer *player = (MediaPlayer *)param;
  player->task();
}

MediaPlayer::MediaPlayer(Display &display, Prefs &prefs, Battery &battery)
    : mDisplay(display), mPrefs(prefs), mBattery(battery)
{
  mMutex = xSemaphoreCreateMutex();
}

MediaPlayer::~MediaPlayer()
{
  if (mTaskHandle != NULL)
  {
    mRunTask = false;
    while (mTaskHandle != NULL)
    {
      vTaskDelay(10);
    }
  }
  if (mCurrentFrame)
  {
    free(mCurrentFrame);
  }
  vSemaphoreDelete(mMutex);
}

void MediaPlayer::start()
{
  // default implementation is to start the task
  startTask();
}

void MediaPlayer::startTask()
{
  mRunTask = true;
  xTaskCreatePinnedToCore(_task, "MediaPlayer", 10000, this, 1,
                          &mTaskHandle, 0);
}

void MediaPlayer::play()
{
  if (mState == MediaPlayerState::PLAYING)
  {
    return;
  }
  auto oldState = mState;
  mState = MediaPlayerState::PLAYING;
  onStateChanged(oldState, mState);
  if (mTaskHandle == NULL)
  {
    startTask();
  }
}

void MediaPlayer::stop()
{
  if (mState == MediaPlayerState::STOPPED)
  {
    return;
  }
  mRunTask = false;
  while (mTaskHandle != NULL)
  {
    vTaskDelay(10);
  }
  auto oldState = mState;
  mState = MediaPlayerState::STOPPED;
  onStateChanged(oldState, mState);
  vTaskDelay(10);
  mDisplay.fillSprite(DisplayColors::BLACK);
  mDisplay.drawOSD("Stopped", CENTER, OSDLevel::STANDARD);
  mDisplay.flushSprite();
}

void MediaPlayer::pause()
{
  if (mState == MediaPlayerState::PAUSED)
  {
    return;
  }
  char batText[12];
  sprintf(batText, mBattery.isCharging() ? "Chrg %d%%" : "Batt. %d%%",
          mBattery.getBatteryLevel());
  drawOSDTimed(std::string(batText), TOP_RIGHT, OSDLevel::STANDARD);
  drawOSDTimed(std::string("Paused"), CENTER, OSDLevel::STANDARD);
  auto oldState = mState;
  mState = MediaPlayerState::PAUSED;
  onStateChanged(oldState, mState);
}

void MediaPlayer::playPauseToggle()
{
  if (mState == MediaPlayerState::PLAYING)
  {
    pause();
  }
  else
  {
    play();
  }
}

void MediaPlayer::drawOSDTimed(const std::string &text, OSDPosition position,
                               OSDLevel level, uint32_t durationMs)
{
  if (text.empty())
    return;
  mTimedOsds.push_back({text, position, level, millis() + durationMs});
  mDisplay.drawOSD(text.c_str(), position, level);
}

void MediaPlayer::task()
{
  uint8_t *jpegBuffer = NULL;
  size_t jpegBufferLength = 0;
  size_t jpegLength = 0;

  while (mRunTask)
  {
    bool needsRedraw = false;
    for (auto it = mTimedOsds.begin(); it != mTimedOsds.end();)
    {
      if (millis() >= it->endTime)
      {
        it = mTimedOsds.erase(it);
        needsRedraw = true;
      }
      else
      {
        ++it;
      }
    }

    if (mState == MediaPlayerState::STATIC)
    {
      onStatic();
      vTaskDelay(20 / portTICK_PERIOD_MS);
      continue;
    }

    bool gotFrame = false;
    if (mState == MediaPlayerState::PLAYING)
    {
      onLoop();
      if (xSemaphoreTake(mMutex, portMAX_DELAY) == pdTRUE)
      {
        gotFrame = getFrame(&jpegBuffer, jpegBufferLength, jpegLength);
        xSemaphoreGive(mMutex);
      }
    }

    // if we don't have a new frame, and we don't need to redraw for OSD, then we can just wait
    if (!gotFrame && !needsRedraw)
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    if (gotFrame)
    {
      if (mCurrentFrame == NULL || jpegLength > mCurrentFrameSize)
      {
        if (mCurrentFrame)
        {
          free(mCurrentFrame);
        }
        mCurrentFrame = (uint8_t *)malloc(jpegLength);
      }
      if (mCurrentFrame)
      {
        mCurrentFrameSize = jpegLength;
        memcpy(mCurrentFrame, jpegBuffer, jpegLength);
      }
    }

    // if we got a frame, or we need to redraw for OSD, then draw
    if (mCurrentFrame)
    {
      mWaitForFirstFrame = false;
      if (mJpeg.openRAM(mCurrentFrame, mCurrentFrameSize, _doDraw))
      {
        mJpeg.setUserPointer(this);
        mJpeg.setPixelType(RGB565_BIG_ENDIAN);
        mJpeg.decode(0, 0, 0);
        mJpeg.close();
      }
    }
    else
    {
      // clear the screen if no frame
      if (!mWaitForFirstFrame)
      {
        mDisplay.fillSprite(DisplayColors::BLACK);
      }
    }

    onFrameDisplayed();

    if (mBattery.isCharging())
    {
      mDisplay.drawOSD("Charging", TOP_RIGHT, OSDLevel::DEBUG);
    }
    else if (mBattery.isLowBattery())
    {
      mDisplay.drawOSD("Low Batt.", TOP_RIGHT, OSDLevel::STANDARD);
    }

    for (const auto &osd : mTimedOsds)
    {
      mDisplay.drawOSD(osd.text.c_str(), osd.position, osd.level);
    }

    mDisplay.flushSprite();
  }

  if (mCurrentFrame)
  {
    free(mCurrentFrame);
    mCurrentFrame = NULL;
    mCurrentFrameSize = 0;
  }

  mTaskHandle = NULL;
  vTaskDelete(NULL);
}
