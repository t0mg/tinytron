#pragma once
#include "VideoSource.h"
#include "MediaPlayer.h"
#include <list>

class VideoPlayer : public MediaPlayer
{
private:
  VideoSource *mVideoSource = NULL;
  std::list<int> mFrameTimes;

protected:
  virtual bool getFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength) override;
  virtual void onFrameDisplayed() override;
  virtual void onStateChanged(MediaPlayerState oldState, MediaPlayerState newState) override;
  virtual void onStatic() override;

public:
  VideoPlayer(VideoSource *videoSource, Display &display, Prefs &prefs,
              Battery &battery);
  virtual void set(int channelIndex) override;
  void playStatic();
  void redrawFrame();

  virtual void next() override;

  virtual void start() override;
};