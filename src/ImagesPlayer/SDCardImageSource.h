#pragma once

#include <string>
#include <vector>

#include "ImageSource.h"

class SDCard;

class SDCardImageSource : public ImageSource {
private:
  std::vector<std::string> mImageFiles;
  SDCard *mSDCard;
  const char *mPath;
  bool mShowFilename;
  int mImageNumber = 0;
  unsigned long mLastChangeTime = 0;
  unsigned long mIntervalMs = 5000;
  bool mForceNext = true;

  bool loadCurrentImage(uint8_t **buffer, size_t &bufferLength,
                        size_t &frameLength);

public:
  SDCardImageSource(SDCard *sdCard, const char *path, bool showFilename = true);
  bool fetchImageData() override;
  int getImageCount() override { return mImageFiles.size(); }
  int getImageNumber() override { return mImageNumber; }
  std::string getImageName() override;
  void setImage(int index) override;
  void nextImage() override;
  bool getImageFrame(uint8_t **buffer, size_t &bufferLength,
                     size_t &frameLength) override;
  uint32_t getAutoAdvanceIntervalMs() override { return (uint32_t)mIntervalMs; }
  bool showImageNameOSD() override { return mShowFilename; }
};
