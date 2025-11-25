#pragma once

#include "VideoSource.h"
#include <ESPAsyncWebServer.h>

enum class StreamState {
  DISCONNECTED,
  CONNECTED,
  STREAMING
};

// Struct to hold JPEG data passed via the FreeRTOS queue
typedef struct
{
  uint8_t *jpeg_data;
  size_t jpeg_len;
} JpegData_t;

class StreamVideoSource: public VideoSource {
  private:
    bool mFrameReady=false;
    uint8_t *mCurrentFrameBuffer = NULL;
    size_t mCurrentFrameLength = 0;
    size_t mCurrentFrameBufferLength = 0;
    AsyncWebServer *mServer = NULL;
    AsyncWebSocket *mWebSocket = NULL;
    SemaphoreHandle_t mCurrentFrameMutex = NULL;
    StreamState mStreamState = StreamState::DISCONNECTED;
    std::vector<uint8_t> mJpegBuffer;
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    SemaphoreHandle_t streamingSemaphore = NULL;
    QueueHandle_t jpegQueue = NULL;
    size_t mCurrentWsFrameLength = 0;

    // static void _frameDownloaderTask(void *arg);
    // void frameDownloaderTask();
  public:
    StreamVideoSource(AsyncWebServer *server);
    void start();
    // see superclass for documentation
    bool getVideoFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength);
    void setChannel(int channel);
    void nextChannel();
    int getChannelCount();
    int getChannelNumber();
    std::string getChannelName() { return "Video stream"; }
    StreamState getStreamState()
    {
        return mStreamState;
    }
    bool fetchChannelData();
};