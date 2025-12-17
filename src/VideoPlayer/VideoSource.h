#pragma once

#include "../MediaPlayer.h"
#include <Arduino.h>
#include <string>

class VideoSource
{
protected:
  MediaPlayerState mState = MediaPlayerState::STOPPED;
  int mAudioTimeMs = 0;
  int mLastAudioTimeUpdateMs = 0;
  int mChannelNumber = 0;

public:
  virtual void start() = 0;
  // Retrieve a JPEG image for the frame at the given time.
  // Returns true if a frame was retrieved, false if the current frame should be
  // re-used e.g. the elapsed time is closest to the previous frame. The buffer
  // is re-allocated if the frame is larger than the previous frame.
  virtual bool getVideoFrame(uint8_t **buffer, size_t &bufferLength,
                             size_t &frameLength) = 0;
  // update the audio time
  void updateAudioTime(int audioTimeMs)
  {
    mAudioTimeMs = audioTimeMs;
    mLastAudioTimeUpdateMs = millis();
  }
  void setState(MediaPlayerState state)
  {
    mState = state;
    switch (state)
    {
    case MediaPlayerState::PLAYING:
      mAudioTimeMs = 0;
      mLastAudioTimeUpdateMs = millis();
      break;
    case MediaPlayerState::PAUSED:
      break;
    case MediaPlayerState::STOPPED:
      mLastAudioTimeUpdateMs = 0;
      break;
    }
  }
  virtual void setChannel(int channel) = 0;
  virtual void nextChannel() = 0;
  virtual int getChannelCount() = 0;
  virtual int getChannelNumber() { return mChannelNumber; }
  virtual std::string getChannelName() = 0;
  virtual bool fetchVideoData() = 0;
};
