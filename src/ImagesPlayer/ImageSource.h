#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

class ImageSource {
public:
  virtual ~ImageSource() = default;
  virtual bool fetchImageData() = 0;
  virtual int getImageCount() = 0;
  virtual int getImageNumber() = 0;
  virtual std::string getImageName() = 0;
  virtual void setImage(int index) = 0;
  virtual void nextImage() = 0;
  virtual bool getImageFrame(uint8_t **buffer, size_t &bufferLength,
                             size_t &frameLength) = 0;
  virtual uint32_t getAutoAdvanceIntervalMs() { return 0; }
  virtual bool showImageNameOSD() { return true; }
};
