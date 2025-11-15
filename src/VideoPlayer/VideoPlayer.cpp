#include <Arduino.h>
#include "VideoPlayer.h"
#include "VideoSource.h"
#include "Display.h"
#include <list>

void VideoPlayer::_framePlayerTask(void *param)
{
  VideoPlayer *player = (VideoPlayer *)param;
  player->framePlayerTask();
}

VideoPlayer::VideoPlayer(VideoSource *videoSource, Display &display)
: mVideoSource(videoSource), mDisplay(display), mState(VideoPlayerState::STOPPED)
{
}

void VideoPlayer::start()
{
  mVideoSource->start();
}

void VideoPlayer::playTask()
{
  m_runTask = true;
  // launch the frame player task
  xTaskCreatePinnedToCore(
      _framePlayerTask,
      "Frame Player",
      10000,
      this,
      1,
      &_framePlayerTaskHandle,
      0);
}

void VideoPlayer::setChannel(int channel)
{
  m_runTask = false;
  // wait for the task to stop
  while (_framePlayerTaskHandle != NULL)
  {
    vTaskDelay(10);
  }
  mChannelVisible = millis();
  // update the video source
  mVideoSource->setChannel(channel);
  playTask();
}

void VideoPlayer::nextChannel()
{
  m_runTask = false;
  // wait for the task to stop
  while (_framePlayerTaskHandle != NULL)
  {
    vTaskDelay(10);
  }
  mChannelVisible = millis();
  mVideoSource->nextChannel();
  playTask();
}

void VideoPlayer::play()
{
  if (mState == VideoPlayerState::PLAYING)
  {
    return;
  }
  mState = VideoPlayerState::PLAYING;
  mVideoSource->setState(VideoPlayerState::PLAYING);
  if (_framePlayerTaskHandle == NULL)
  {
    playTask();
  }
}

void VideoPlayer::stop()
{
  if (mState == VideoPlayerState::STOPPED)
  {
    return;
  }
  m_runTask = false;
  // wait for the task to stop
  while (_framePlayerTaskHandle != NULL)
  {
    vTaskDelay(10);
  }
  mState = VideoPlayerState::STOPPED;
  mVideoSource->setState(VideoPlayerState::STOPPED);
  mDisplay.fillScreen(DisplayColors::BLACK);
}

void VideoPlayer::pause()
{
  if (mState == VideoPlayerState::PAUSED)
  {
    return;
  }
  mState = VideoPlayerState::PAUSED;
  mVideoSource->setState(VideoPlayerState::PAUSED);
}

void VideoPlayer::playPauseToggle()
{
  if (mState == VideoPlayerState::PLAYING)
  {
    pause();
  }
  else
  {
    play();
  }
}

void VideoPlayer::playStatic()
{
  if (mState == VideoPlayerState::STATIC)
  {
    return;
  }

  // If a task is running, stop it cleanly
  if (_framePlayerTaskHandle != NULL)
  {
    m_runTask = false;
    while (_framePlayerTaskHandle != NULL)
    {
      vTaskDelay(10);
    }
  }

  mState = VideoPlayerState::STATIC;
  mVideoSource->setState(VideoPlayerState::STATIC);

  mDisplay.fillScreen(DisplayColors::BLACK); // Added to reset display state

  // Start the task in STATIC mode
  playTask();
}


// double buffer the dma drawing otherwise we get corruption
uint16_t *dmaBuffer[2] = {NULL, NULL};
int dmaBufferIndex = 0;
int _doDraw(JPEGDRAW *pDraw)
{
  VideoPlayer *player = (VideoPlayer *)pDraw->pUser;
  player->mDisplay.drawPixels(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
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

void VideoPlayer::framePlayerTask()
{
  uint16_t *staticBuffer = NULL;
  uint8_t *jpegBuffer = NULL;
  size_t jpegBufferLength = 0;
  size_t jpegLength = 0;
  // used for calculating frame rate
  std::list<int> frameTimes;
  while (m_runTask)
  {
    if (mState == VideoPlayerState::STOPPED || mState == VideoPlayerState::PAUSED)
    {
      // nothing to do - just wait
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    if (mState == VideoPlayerState::STATIC)
    {
      // draw random pixels to the screen to simulate static
      // we'll do this 8 rows of pixels at a time to save RAM
      int width = mDisplay.width();
      int height = 8;
      if (staticBuffer == NULL)
      {
        staticBuffer = (uint16_t *)malloc(width * height * 2);
      }
      mDisplay.startWrite();
      for (int i = 0; i < mDisplay.height(); i++)
      {
        if (!m_runTask)
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
      mDisplay.endWrite();
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }
    // get the next frame
    if (!mVideoSource->getVideoFrame(&jpegBuffer, jpegBufferLength, jpegLength))
    {
      // no frame ready yet
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    frameTimes.push_back(millis());
    // keep the frame rate elapsed time to 5 seconds
    while(frameTimes.size() > 0 && frameTimes.back() - frameTimes.front() > 5000) {
      frameTimes.pop_front();
    }
    mDisplay.startWrite();
    if (mJpeg.openRAM(jpegBuffer, jpegLength, _doDraw))
    {
      mJpeg.setUserPointer(this);
      #ifdef LED_MATRIX
      mJpeg.setPixelType(RGB565_LITTLE_ENDIAN);
      #else
      mJpeg.setPixelType(RGB565_BIG_ENDIAN);
      #endif
      mJpeg.decode(0, 0, 0);
      mJpeg.close();
    }
    // show channel indicator 
    if (millis() - mChannelVisible < 2000) {
      mDisplay.drawChannel(mVideoSource->getChannelNumber());
    }
    mDisplay.endWrite();
  }
  // clean up
  if (staticBuffer != NULL)
  {
    free(staticBuffer);
  }
  _framePlayerTaskHandle = NULL;
  vTaskDelete(NULL);
}