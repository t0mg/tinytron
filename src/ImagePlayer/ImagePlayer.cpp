#include "ImagePlayer.h"

#include "../Battery.h"
#include "../Display.h"
#include "../Prefs.h"
#include "ImageSource.h"
#include <Arduino.h>

static void fadeBacklight(Display &display, int fromBrightness,
                          int toBrightness, int steps, int delayMs)
{
  if (steps <= 0)
  {
    display.setBrightness(toBrightness);
    return;
  }
  for (int i = 0; i <= steps; i++)
  {
    int v = fromBrightness + ((toBrightness - fromBrightness) * i) / steps;
    display.setBrightness((uint8_t)constrain(v, 0, 255));
    vTaskDelay(delayMs / portTICK_PERIOD_MS);
  }
}

static const int kFadeSteps = 50;
static const int kFadeDelayMs = 20;

void ImagePlayer::start()
{
  // no-op
}

ImagePlayer::ImagePlayer(ImageSource *imageSource, Display &display,
                         Prefs &prefs, Battery &battery)
    : MediaPlayer(display, prefs, battery),
      mImageSource(imageSource)
{
  mLastAdvanceMs = millis();
}

void ImagePlayer::set(int index)
{
  if (!mImageSource)
  {
    return;
  }
  if (xSemaphoreTake(mMutex, portMAX_DELAY) == pdTRUE)
  {
    mImageSource->setImage(index);
    mLastAdvanceMs = millis();
    xSemaphoreGive(mMutex);
  }
}

void ImagePlayer::next()
{
  if (!mImageSource)
  {
    return;
  }
  // Fade out the current image before advancing
  int targetBrightness = mPrefs.getBrightness();
  fadeBacklight(mDisplay, targetBrightness, 0, kFadeSteps, kFadeDelayMs);

  if (xSemaphoreTake(mMutex, portMAX_DELAY) == pdTRUE)
  {
    mImageSource->nextImage();
    mLastAdvanceMs = millis();
    xSemaphoreGive(mMutex);
  }
}

bool ImagePlayer::getFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength)
{
  if (!mImageSource)
  {
    return false;
  }
  return mImageSource->getImageFrame(buffer, bufferLength, frameLength);
}

void ImagePlayer::onLoop()
{
  // Auto-advance
  uint32_t intervalMs = mPrefs.getSlideshowInterval() * 1000;
  if (intervalMs > 0)
  {
    uint32_t now = millis();
    if ((uint32_t)(now - mLastAdvanceMs) >= intervalMs)
    {
      next();
    }
  }
}

void ImagePlayer::onFrameDisplayed()
{
  int currentIndex = mImageSource->getImageNumber();
  bool changed =
      (lastRenderedIndex != -1 && currentIndex != lastRenderedIndex);

  // Update index now to prevent re-entry issues
  lastRenderedIndex = currentIndex;

  if (changed)
  {
    // The new frame has been decoded to the sprite by the MediaPlayer task.
    // The screen is currently black because next() faded it out.

    // Draw our image-specific OSD to the sprite
    if (mImageSource->showImageNameOSD())
    {
      drawOSDTimed(mImageSource->getImageName(), TOP_LEFT, OSDLevel::STANDARD);
    }

    // Manually flush the sprite to get the new image (and its OSD) on screen.
    mDisplay.flushSprite();

    // Now that the new image is on the screen (but invisible), fade it in.
    int targetBrightness = mPrefs.getBrightness();
    fadeBacklight(mDisplay, 0, targetBrightness, kFadeSteps, kFadeDelayMs);
  }
}
