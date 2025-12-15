#include "VideoPlayer.h"
#include "Display.h"
#include "Prefs.h"
#include "VideoSource.h"
#include <Arduino.h>
#include <list>

void VideoPlayer::_framePlayerTask(void *param) {
  VideoPlayer *player = (VideoPlayer *)param;
  player->framePlayerTask();
}

VideoPlayer::VideoPlayer(VideoSource *videoSource, Display &display,
                         Prefs &prefs, Battery &battery)
    : mVideoSource(videoSource), mDisplay(display), mPrefs(prefs),
      mBattery(battery), mState(VideoPlayerState::STOPPED) {}

void VideoPlayer::start() { mVideoSource->start(); }

void VideoPlayer::playTask() {
  m_runTask = true;
  // launch the frame player task
  xTaskCreatePinnedToCore(_framePlayerTask, "Frame Player", 10000, this, 1,
                          &_framePlayerTaskHandle, 0);
}

void VideoPlayer::setChannel(int channel) {
  m_runTask = false;
  // wait for the task to stop
  while (_framePlayerTaskHandle != NULL) {
    vTaskDelay(10);
  }
  // update the video source
  mVideoSource->setChannel(channel);
  drawOSDTimed(mVideoSource->getChannelName(), TOP_LEFT, OSDLevel::STANDARD);
  playTask();
}

void VideoPlayer::nextChannel() {
  if (mState == VideoPlayerState::PAUSED) {
    play();
  }

  m_runTask = false;
  // wait for the task to stop
  while (_framePlayerTaskHandle != NULL) {
    vTaskDelay(10);
  }
  mVideoSource->nextChannel();
  drawOSDTimed(mVideoSource->getChannelName(), TOP_LEFT, OSDLevel::STANDARD);
  playTask();
}

void VideoPlayer::play() {
  if (mState == VideoPlayerState::PLAYING) {
    return;
  }
  mState = VideoPlayerState::PLAYING;
  mVideoSource->setState(VideoPlayerState::PLAYING);
  if (_framePlayerTaskHandle == NULL) {
    playTask();
  }
}

void VideoPlayer::stop() {
  if (mState == VideoPlayerState::STOPPED) {
    return;
  }
  m_runTask = false;
  // wait for the task to stop
  while (_framePlayerTaskHandle != NULL) {
    vTaskDelay(10);
  }
  mState = VideoPlayerState::STOPPED;
  mVideoSource->setState(VideoPlayerState::STOPPED);
  vTaskDelay(10);
  mDisplay.fillSprite(DisplayColors::BLACK);
  mDisplay.drawOSD("Stopped", CENTER, OSDLevel::STANDARD);
  mDisplay.flushSprite();
}

void VideoPlayer::pause() {
  Serial.println("Pausing");
  if (mState == VideoPlayerState::PAUSED) {
    return;
  }
  char batText[12];
  sprintf(batText, mBattery.isCharging() ? "Chrg %d%%" : "Batt. %d%%",
          mBattery.getBatteryLevel());
  drawOSDTimed(std::string(batText), TOP_RIGHT, OSDLevel::STANDARD);
  drawOSDTimed(std::string("Paused"), CENTER, OSDLevel::STANDARD);
  mState = VideoPlayerState::PAUSED;
  mVideoSource->setState(VideoPlayerState::PAUSED);
  Serial.println("Paused");
}

void VideoPlayer::playPauseToggle() {
  if (mState == VideoPlayerState::PLAYING) {
    pause();
  } else {
    play();
  }
}

void VideoPlayer::playStatic() {
  if (mState == VideoPlayerState::STATIC) {
    return;
  }

  // If a task is running, stop it cleanly
  if (_framePlayerTaskHandle != NULL) {
    m_runTask = false;
    while (_framePlayerTaskHandle != NULL) {
      vTaskDelay(10);
    }
  }

  mState = VideoPlayerState::STATIC;
  mVideoSource->setState(VideoPlayerState::STATIC);

  mDisplay.fillScreen(DisplayColors::BLACK);

  // Start the task in STATIC mode
  playTask();
}

// double buffer the dma drawing otherwise we get corruption
uint16_t *dmaBuffer[2] = {NULL, NULL};
int dmaBufferIndex = 0;
int _doDraw(JPEGDRAW *pDraw) {
  VideoPlayer *player = (VideoPlayer *)pDraw->pUser;
  // calculate the x offset to center the image if encoded at 288 pixels for
  // faster JPEG decoding
  int x_offset = 0;
  int imageWidth = player->mJpeg.getWidth();
  int screenWidth = player->mDisplay.width();
  x_offset = (screenWidth - imageWidth) / 2;
  player->mDisplay.drawPixelsToSprite(pDraw->x + x_offset, pDraw->y,
                                      pDraw->iWidth, pDraw->iHeight,
                                      pDraw->pPixels);
  return 1;
}

static unsigned short x = 12345, y = 6789, z = 42, w = 1729;

unsigned short xorshift16() {
  unsigned short t = x ^ (x << 5);
  x = y;
  y = z;
  z = w;
  w = w ^ (w >> 1) ^ t ^ (t >> 3);
  return w & 0xFFFF;
}

void VideoPlayer::framePlayerTask() {
  uint16_t *staticBuffer = NULL;
  uint8_t *jpegBuffer = NULL;
  size_t jpegBufferLength = 0;
  size_t jpegLength = 0;
  // used for calculating frame rate
  std::list<int> frameTimes;
  OSDLevel osdLevel = mPrefs.getOsdLevel();
  while (m_runTask) {
    // handle timed OSDs
    bool needsRedraw = false;
    for (auto it = _timedOsds.begin(); it != _timedOsds.end();) {
      if (millis() >= it->endTime) {
        it = _timedOsds.erase(it);
        needsRedraw = true;
      } else {
        ++it;
      }
    }
    if (needsRedraw) {
      redrawFrame();
    }

    if (mState == VideoPlayerState::STOPPED) {
      //   // draw the paused OSD over the current frame
      //   // drawOSDTimed("Paused", CENTER, OSDLevel::STANDARD);
      //   // push the result to the screen
      //   // mDisplay.flushSprite();
      //   // now wait until we are un-paused
      //   while (mState == VideoPlayerState::PAUSED)
      //   {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      //     if (!m_runTask)
      //       break; // allow task to be stopped while paused
      //   }
      continue;
    }
    if (mState == VideoPlayerState::STATIC) {
      // draw random pixels to the screen to simulate static
      // we'll do this 8 rows of pixels at a time to save RAM
      int width = mDisplay.width();
      int height = 8;
      if (staticBuffer == NULL) {
        staticBuffer = (uint16_t *)malloc(width * height * 2);
      }
      for (int i = 0; i < mDisplay.height(); i++) {
        if (!m_runTask) {
          break;
        }
        for (int p = 0; p < width * height; p++) {
          int grey = xorshift16() >> 8;
          staticBuffer[p] = mDisplay.color565(grey, grey, grey);
        }
        mDisplay.drawPixels(0, i * height, width, height, staticBuffer);
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }
    // get the next frame
    if (!mVideoSource->getVideoFrame(&jpegBuffer, jpegBufferLength,
                                     jpegLength)) {
      // no frame ready yet
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    // store the current frame for redraw
    if (_currentFrame == NULL || jpegLength > _currentFrameSize) {
      if (_currentFrame) {
        free(_currentFrame);
      }
      _currentFrame = (uint8_t *)malloc(jpegLength);
    }
    if (_currentFrame) {
      _currentFrameSize = jpegLength;
      memcpy(_currentFrame, jpegBuffer, jpegLength);
    }

    if (osdLevel >= OSDLevel::DEBUG) {
      frameTimes.push_back(millis());
      // keep the frame rate elapsed time to 5 seconds
      while (frameTimes.size() > 0 &&
             frameTimes.back() - frameTimes.front() > 5000) {
        frameTimes.pop_front();
      }
    }
    if (mJpeg.openRAM(jpegBuffer, jpegLength, _doDraw)) {
      mJpeg.setUserPointer(this);
      mJpeg.setPixelType(RGB565_BIG_ENDIAN);
      mJpeg.decode(0, 0, 0);
      mJpeg.close();
    }
    if (osdLevel >= OSDLevel::DEBUG) {
      char fpsText[8];
      sprintf(fpsText, "%d FPS", frameTimes.size() / 5);
      mDisplay.drawOSD(fpsText, BOTTOM_RIGHT, OSDLevel::DEBUG);
      char batText[16];
      sprintf(batText, "%d%% %.2f", mBattery.getBatteryLevel(),
              mBattery.getVoltage());
      mDisplay.drawOSD(batText, BOTTOM_LEFT, OSDLevel::DEBUG);
    }

    if (mBattery.isCharging()) {
      mDisplay.drawOSD("Charging", TOP_RIGHT, OSDLevel::DEBUG);
    } else if (mBattery.isLowBattery()) {
      mDisplay.drawOSD("Low Batt.", TOP_RIGHT, OSDLevel::STANDARD);
    }

    for (const auto &osd : _timedOsds) {
      mDisplay.drawOSD(osd.text.c_str(), osd.position, osd.level);
    }
    mDisplay.flushSprite();
  }
  // clean up
  if (staticBuffer != NULL) {
    free(staticBuffer);
  }
  if (_currentFrame != NULL) {
    free(_currentFrame);
    _currentFrame = NULL;
  }
  _framePlayerTaskHandle = NULL;
  vTaskDelete(NULL);
}

void VideoPlayer::drawOSDTimed(const std::string &text, OSDPosition position,
                               OSDLevel level, uint32_t durationMs) {
  _timedOsds.push_back({text, position, level, millis() + durationMs});
  // immediately draw the OSD
  mDisplay.drawOSD(text.c_str(), position, level);
}

void VideoPlayer::redrawFrame() {
  if (_currentFrame) {
    if (mJpeg.openRAM(_currentFrame, _currentFrameSize, _doDraw)) {
      mJpeg.setUserPointer(this);
      mJpeg.setPixelType(RGB565_BIG_ENDIAN);
      mJpeg.decode(0, 0, 0);
      mJpeg.close();
    }
    mDisplay.flushSprite();
  } else {
    mDisplay.fillScreen(DisplayColors::BLACK);
  }
}