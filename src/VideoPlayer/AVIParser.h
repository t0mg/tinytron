#pragma once

#include <stdio.h>
#include <string>

enum class AVIChunkType
{
  VIDEO,
  AUDIO
};

class AVIParser
{
private:
  std::string mFileName;
  AVIChunkType mRequiredChunkType;
  FILE *mFile = NULL;
  long mMoviListPosition = 0;
  long mMoviListLength;
  float mFrameRate = 0;

public:
  AVIParser(std::string fname, AVIChunkType requiredChunkType);
  ~AVIParser();
  bool open();
  size_t getNextChunk(uint8_t **buffer, size_t &bufferLength);
  float getFrameRate() { return mFrameRate; };
};