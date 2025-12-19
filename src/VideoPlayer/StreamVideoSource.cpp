#include "StreamVideoSource.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

const int MIN_FRAME_INTERVAL_MS = 1000 / 30; // approx 30fps

StreamVideoSource::StreamVideoSource(AsyncWebServer *server) : mServer(server)
{
  mWebSocket = new AsyncWebSocket("/ws");
  mServer->addHandler(mWebSocket);
  mWebSocket->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
                      {
    mLastReadyTime = 0;
    onWsEvent(server, client, type, arg, data, len); });
}

void StreamVideoSource::start()
{
  // create a mutex to control access to the JPEG buffer
  mCurrentFrameMutex = xSemaphoreCreateMutex();
  streamingSemaphore = xSemaphoreCreateMutex();
  jpegQueue = xQueueCreate(10, sizeof(JpegData_t));
}

bool StreamVideoSource::getVideoFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength)
{
  if (mStreamState != StreamState::STREAMING)
  {
    return false;
  }

  bool copiedFrame = false;
  // lock the image buffer
  xSemaphoreTake(mCurrentFrameMutex, portMAX_DELAY);
  // if the frame is ready, copy it to the buffer
  JpegData_t receivedJpeg;
  if (xQueueReceive(jpegQueue, &receivedJpeg, portMAX_DELAY) == pdPASS)
  {
    copiedFrame = true;
    // reallocate the image buffer if necessary
    if (receivedJpeg.jpeg_len > bufferLength)
    {
      *buffer = (uint8_t *)realloc(*buffer, receivedJpeg.jpeg_len);
      bufferLength = receivedJpeg.jpeg_len;
    }
    // copy the image buffer
    memcpy(*buffer, receivedJpeg.jpeg_data, receivedJpeg.jpeg_len);
    frameLength = receivedJpeg.jpeg_len;
    // free the memory that was allocated in the websocket handler
    free(receivedJpeg.jpeg_data);

    // Send "ready"
    uint32_t now = millis();
    if (now - mLastReadyTime > MIN_FRAME_INTERVAL_MS)
    {
      if (xSemaphoreTake(streamingSemaphore, portMAX_DELAY) == pdTRUE)
      {
        if (mStreamState == StreamState::STREAMING)
        {
          mWebSocket->textAll("ready");
          mLastReadyTime = now;
        }
        xSemaphoreGive(streamingSemaphore);
      }
    }
  }
  // unlock the image buffer
  xSemaphoreGive(mCurrentFrameMutex);
  // return true if we copied a frame, false otherwise
  return copiedFrame;
}

void StreamVideoSource::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    mStreamState = StreamState::CONNECTED;
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    mStreamState = StreamState::DISCONNECTED;
    mJpegBuffer.clear(); // Clear buffer on disconnect
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode == WS_TEXT)
    {
      if (len == 5 && strncmp((char *)data, "START", 5) == 0)
      {
        Serial.println("Received START command");

        if (xSemaphoreTake(streamingSemaphore, portMAX_DELAY) == pdTRUE)
        {
          mStreamState = StreamState::STREAMING;
          xSemaphoreGive(streamingSemaphore);
          mWebSocket->textAll("ready");
        }
      }
      else if (len == 4 && strncmp((char *)data, "STOP", 4) == 0)
      {
        Serial.println("Received STOP command");

        if (xSemaphoreTake(streamingSemaphore, portMAX_DELAY) == pdTRUE)
        {
          mStreamState = StreamState::CONNECTED;
          xSemaphoreGive(streamingSemaphore);

          // Attendre un petit moment pour que la dernière transaction se termine
          vTaskDelay(pdMS_TO_TICKS(100));

          // Vider la queue de manière sécurisée
          JpegData_t dummyData;
          while (xQueueReceive(jpegQueue, &dummyData, 0) == pdTRUE)
          {
            if (dummyData.jpeg_data != NULL)
            {
              free(dummyData.jpeg_data);
            }
          }
          mJpegBuffer.clear();
        }
      }
      return;
    }

    // Check if it's the start of a new message
    // info->index == 0 means it's the first frame
    if (info->index == 0)
    {
      Serial.printf("New binary message, total len: %u\n", info->len);
      mCurrentWsFrameLength = info->len;
      mJpegBuffer.clear();
      mJpegBuffer.reserve(info->len);
    }

    // Append the current frame's data to our buffer
    // 'data' is the buffer for this frame, 'len' is its size
    mJpegBuffer.insert(mJpegBuffer.end(), data, data + len);
    Serial.printf("Appended %d bytes, total buffer: %d\n", len, mJpegBuffer.size());

    // Check if this is the final frame
    if (mJpegBuffer.size() >= mCurrentWsFrameLength)
    {
      Serial.printf("Final frame received. Total size: %d bytes. Decoding...\n", mJpegBuffer.size());

      if (uxQueueSpacesAvailable(jpegQueue) > 0)
      {
        // 1. Allocate new memory for the decoder task
        // We use malloc here because it's thread-safe and not tied to the std::vector
        uint8_t *jpeg_copy = (uint8_t *)malloc(mJpegBuffer.size());

        if (jpeg_copy != NULL)
        {
          // 2. Copy the data from the vector into the new buffer
          memcpy(jpeg_copy, mJpegBuffer.data(), mJpegBuffer.size());

          // 3. Create the struct to send
          JpegData_t newJpeg = {.jpeg_data = jpeg_copy, .jpeg_len = mJpegBuffer.size()};

          // 4. Send the struct (containing the *new* pointer) to the queue
          if (xQueueSend(jpegQueue, &newJpeg, 0) != pdPASS)
          {
            // This shouldn't happen if we just checked, but it's good practice
            Serial.println("Failed to queue frame, freeing copy.");
            free(jpeg_copy); // Clean up if send fails
          }
          else
          {
            // Serial.println("Frame queued successfully.");
            // Ownership is now transferred to the decoder task
          }
        }
        else
        {
          Serial.println("malloc failed! Dropping frame.");
          // Not enough RAM to make a copy, drop the frame
        }
      }
      else
      {
        Serial.println("Queue full, dropping frame.");
      }

      // IMPORTANT: Clear the buffer to be ready for the next image
      mJpegBuffer.clear();
      mCurrentWsFrameLength = 0;
    }
  }
}

void StreamVideoSource::setChannel(int channel)
{
}

void StreamVideoSource::nextChannel()
{
}

int StreamVideoSource::getChannelCount()
{
  return 1;
}

int StreamVideoSource::getChannelNumber()
{
  return 0;
}

bool StreamVideoSource::fetchVideoData()
{
  return true;
}