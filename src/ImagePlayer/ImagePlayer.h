#pragma once

#include "ImageSource.h"
#include "../MediaPlayer.h"

class ImagePlayer : public MediaPlayer
{
private:
  ImageSource *mImageSource = NULL;
  uint32_t mLastAdvanceMs = 0;
  int lastRenderedIndex = -1;

protected:
  virtual bool getFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength) override;
  virtual void onFrameDisplayed() override;
  virtual void onLoop() override;

public:
  ImagePlayer(ImageSource *imageSource, Display &display, Prefs &prefs,
              Battery &battery);

  virtual void set(int index) override;
  virtual void next() override;
  virtual void start() override;
};
