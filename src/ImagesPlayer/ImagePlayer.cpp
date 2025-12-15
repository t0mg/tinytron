#include "ImagePlayer.h"

#include "../Battery.h"
#include "../Display.h"
#include "../Prefs.h"
#include "ImageSource.h"
#include <Arduino.h>

static void fadeBacklight(Display &display, int fromBrightness,
                          int toBrightness, int steps, int delayMs) {
  if (steps <= 0) {
    display.setBrightness(toBrightness);
    return;
  }
  for (int i = 0; i <= steps; i++) {
    int v = fromBrightness + ((toBrightness - fromBrightness) * i) / steps;
    display.setBrightness((uint8_t)constrain(v, 0, 255));
    vTaskDelay(delayMs / portTICK_PERIOD_MS);
  }
}

static const int kFadeSteps = 50;
static const int kFadeDelayMs = 20;

void ImagePlayer::_frameTask(void *param) {
  ImagePlayer *player = (ImagePlayer *)param;
  player->frameTask();
}

ImagePlayer::ImagePlayer(ImageSource *imageSource, Display &display,
                         Prefs &prefs, Battery &battery)
    : mDisplay(display), mPrefs(prefs), mBattery(battery),
      mImageSource(imageSource) {
  mMutex = xSemaphoreCreateMutex();
  mLastAdvanceMs = millis();
}

void ImagePlayer::start() {
  // no-op (source fetch happens in main)
}

void ImagePlayer::startTask() {
  mRunTask = true;
  xTaskCreatePinnedToCore(_frameTask, "Image Frame Player", 10000, this, 1,
                          &mFrameTaskHandle, 0);
}

void ImagePlayer::play() {
  if (mState == ImagePlayerState::PLAYING) {
    return;
  }
  mState = ImagePlayerState::PLAYING;
  if (mFrameTaskHandle == NULL) {
    startTask();
  }
}

void ImagePlayer::pause() {
  if (mState == ImagePlayerState::PAUSED) {
    return;
  }
  char batText[12];
  sprintf(batText, mBattery.isCharging() ? "Chrg %d%%" : "Batt. %d%%",
          mBattery.getBatteryLevel());
  drawOSDTimed(std::string(batText), TOP_RIGHT, OSDLevel::STANDARD);
  drawOSDTimed(std::string("Paused"), CENTER, OSDLevel::STANDARD);
  mState = ImagePlayerState::PAUSED;
}

void ImagePlayer::stop() {
  if (mState == ImagePlayerState::STOPPED) {
    return;
  }
  mRunTask = false;
  while (mFrameTaskHandle != NULL) {
    vTaskDelay(10);
  }
  mState = ImagePlayerState::STOPPED;
  vTaskDelay(10);
  mDisplay.fillSprite(DisplayColors::BLACK);
  mDisplay.drawOSD("Stopped", CENTER, OSDLevel::STANDARD);
  mDisplay.flushSprite();
}

void ImagePlayer::playPauseToggle() {
  if (mState == ImagePlayerState::PLAYING) {
    pause();
  } else {
    play();
  }
}

void ImagePlayer::setImage(int index) {
  if (!mImageSource) {
    return;
  }
  if (xSemaphoreTake(mMutex, portMAX_DELAY) == pdTRUE) {
    mImageSource->setImage(index);
    mLastAdvanceMs = millis();
    xSemaphoreGive(mMutex);
  }
}

void ImagePlayer::nextImage() {
  if (!mImageSource) {
    return;
  }
  if (xSemaphoreTake(mMutex, portMAX_DELAY) == pdTRUE) {
    mImageSource->nextImage();
    mLastAdvanceMs = millis();
    xSemaphoreGive(mMutex);
  }
}

// double buffer the dma drawing otherwise we get corruption
static uint16_t *dmaBuffer[2] = {NULL, NULL};
static int dmaBufferIndex = 0;

int _doDrawImage(JPEGDRAW *pDraw) {
  ImagePlayer *player = (ImagePlayer *)pDraw->pUser;
  int x_offset = 0;
  int imageWidth = player->mJpeg.getWidth();
  int screenWidth = player->mDisplay.width();
  x_offset = (screenWidth - imageWidth) / 2;

  player->mDisplay.drawPixelsToSprite(pDraw->x + x_offset, pDraw->y,
                                      pDraw->iWidth, pDraw->iHeight,
                                      pDraw->pPixels);
  return 1;
}

void ImagePlayer::frameTask() {
  uint8_t *jpegBuffer = NULL;
  size_t jpegBufferLength = 0;
  size_t jpegLength = 0;

  int lastRenderedIndex = -1;

  while (mRunTask) {
    // timed OSD expiry
    bool needsRedraw = false;
    for (auto it = mTimedOsds.begin(); it != mTimedOsds.end();) {
      if (millis() >= it->endTime) {
        it = mTimedOsds.erase(it);
        needsRedraw = true;
      } else {
        ++it;
      }
    }

    if (mState != ImagePlayerState::PLAYING) {
      vTaskDelay(20 / portTICK_PERIOD_MS);
      continue;
    }

    if (!mImageSource) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    // Auto-advance
    uint32_t intervalMs = mImageSource->getAutoAdvanceIntervalMs();
    if (intervalMs > 0) {
      uint32_t now = millis();
      if ((uint32_t)(now - mLastAdvanceMs) >= intervalMs) {
        nextImage();
      }
    }

    int currentIndex = mImageSource->getImageNumber();
    bool changed =
        (lastRenderedIndex != -1 && currentIndex != lastRenderedIndex);

    int targetBrightness = mPrefs.getBrightness();
    if (changed) {
      fadeBacklight(mDisplay, targetBrightness, 0, kFadeSteps, kFadeDelayMs);
    }

    bool gotFrame = false;
    if (xSemaphoreTake(mMutex, portMAX_DELAY) == pdTRUE) {
      gotFrame = mImageSource->getImageFrame(&jpegBuffer, jpegBufferLength,
                                             jpegLength);
      xSemaphoreGive(mMutex);
    }

    if (!gotFrame) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // store for redraw
    if (mCurrentFrame == NULL || jpegLength > mCurrentFrameSize) {
      if (mCurrentFrame) {
        free(mCurrentFrame);
      }
      mCurrentFrame = (uint8_t *)malloc(jpegLength);
    }
    if (mCurrentFrame) {
      mCurrentFrameSize = jpegLength;
      memcpy(mCurrentFrame, jpegBuffer, jpegLength);
    }

    if (mJpeg.openRAM(jpegBuffer, jpegLength, _doDrawImage)) {
      mJpeg.setUserPointer(this);
      mJpeg.setPixelType(RGB565_BIG_ENDIAN);
      mJpeg.decode(0, 0, 0);
      mJpeg.close();
    }

    if (changed && mImageSource->showImageNameOSD()) {
      drawOSDTimed(mImageSource->getImageName(), TOP_LEFT, OSDLevel::STANDARD);
    }

    if (mBattery.isCharging()) {
      mDisplay.drawOSD("Charging", TOP_RIGHT, OSDLevel::DEBUG);
    } else if (mBattery.isLowBattery()) {
      mDisplay.drawOSD("Low Batt.", TOP_RIGHT, OSDLevel::STANDARD);
    }

    for (const auto &osd : mTimedOsds) {
      mDisplay.drawOSD(osd.text.c_str(), osd.position, osd.level);
    }

    mDisplay.flushSprite();

    if (changed) {
      fadeBacklight(mDisplay, 0, targetBrightness, kFadeSteps, kFadeDelayMs);
    }

    lastRenderedIndex = currentIndex;

    if (needsRedraw) {
      // redraw current frame
      if (mCurrentFrame) {
        if (mJpeg.openRAM(mCurrentFrame, mCurrentFrameSize, _doDrawImage)) {
          mJpeg.setUserPointer(this);
          mJpeg.setPixelType(RGB565_BIG_ENDIAN);
          mJpeg.decode(0, 0, 0);
          mJpeg.close();
        }
        mDisplay.flushSprite();
      }
    }
  }

  if (mCurrentFrame) {
    free(mCurrentFrame);
    mCurrentFrame = NULL;
    mCurrentFrameSize = 0;
  }

  mFrameTaskHandle = NULL;
  vTaskDelete(NULL);
}

void ImagePlayer::drawOSDTimed(const std::string &text, OSDPosition position,
                               OSDLevel level, uint32_t durationMs) {
  mTimedOsds.push_back({text, position, level, millis() + durationMs});
  mDisplay.drawOSD(text.c_str(), position, level);
}
