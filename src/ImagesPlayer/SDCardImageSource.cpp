#include "SDCardImageSource.h"
#include "../SDCard.h"
#include <Arduino.h>
#include <algorithm>
#include <stdio.h>

SDCardImageSource::SDCardImageSource(SDCard *sdCard, const char *path,
                                     bool showFilename)
    : mSDCard(sdCard), mPath(path), mShowFilename(showFilename) {}

bool SDCardImageSource::fetchImageData() {
  if (!mSDCard->isMounted()) {
    Serial.println("SD card is not mounted");
    return false;
  }

  mImageFiles.clear();

  auto jpg = mSDCard->listFiles(mPath, ".jpg");
  auto jpeg = mSDCard->listFiles(mPath, ".jpeg");
  auto JPG = mSDCard->listFiles(mPath, ".JPG");
  auto JPEG = mSDCard->listFiles(mPath, ".JPEG");

  mImageFiles.insert(mImageFiles.end(), jpg.begin(), jpg.end());
  mImageFiles.insert(mImageFiles.end(), jpeg.begin(), jpeg.end());
  mImageFiles.insert(mImageFiles.end(), JPG.begin(), JPG.end());
  mImageFiles.insert(mImageFiles.end(), JPEG.begin(), JPEG.end());

  std::sort(mImageFiles.begin(), mImageFiles.end());
  mImageFiles.erase(std::unique(mImageFiles.begin(), mImageFiles.end()),
                    mImageFiles.end());

  if (mImageFiles.empty()) {
    Serial.println("No image files found");
    return false;
  }

  mImageNumber = 0;
  mLastChangeTime = 0;
  mForceNext = true;
  return true;
}

void SDCardImageSource::setImage(int index) {
  if (mImageFiles.empty()) {
    return;
  }

  if (index < 0) {
    index = 0;
  }
  if (index >= (int)mImageFiles.size()) {
    index = (int)mImageFiles.size() - 1;
  }

  mImageNumber = index;
  mLastChangeTime = millis();
  mForceNext = true;
}

void SDCardImageSource::nextImage() {
  if (mImageFiles.empty()) {
    return;
  }

  int index = mImageNumber + 1;
  if (index >= (int)mImageFiles.size()) {
    index = 0;
  }

  setImage(index);
}

std::string SDCardImageSource::getImageName() {
  if (mImageNumber >= 0 && mImageNumber < (int)mImageFiles.size()) {
    std::string fullPath = mImageFiles[mImageNumber];
    size_t lastSlash = fullPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
      return fullPath.substr(lastSlash + 1);
    }
    return fullPath;
  }
  return "Unknown";
}

bool SDCardImageSource::loadCurrentImage(uint8_t **buffer, size_t &bufferLength,
                                         size_t &frameLength) {
  if (mImageNumber < 0 || mImageNumber >= (int)mImageFiles.size()) {
    return false;
  }

  const std::string &filename = mImageFiles[mImageNumber];
  FILE *f = fopen(filename.c_str(), "rb");
  if (!f) {
    Serial.printf("Failed to open image file %s\n", filename.c_str());
    return false;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (size <= 0) {
    fclose(f);
    return false;
  }
  rewind(f);

  if ((size_t)size > bufferLength) {
    *buffer = (uint8_t *)realloc(*buffer, (size_t)size);
    bufferLength = (size_t)size;
  }

  size_t readCount = fread(*buffer, 1, (size_t)size, f);
  fclose(f);

  if (readCount != (size_t)size) {
    Serial.printf("Short read for %s\n", filename.c_str());
    return false;
  }

  frameLength = (size_t)size;
  return true;
}

bool SDCardImageSource::getImageFrame(uint8_t **buffer, size_t &bufferLength,
                                      size_t &frameLength) {
  if (mImageFiles.empty()) {
    return false;
  }

  // For still images, only emit a frame when forced by a channel change.
  // VideoPlayer owns the slideshow timer to avoid conflicts with manual next.
  if (!mForceNext) {
    return false;
  }

  mForceNext = false;
  return loadCurrentImage(buffer, bufferLength, frameLength);
}
