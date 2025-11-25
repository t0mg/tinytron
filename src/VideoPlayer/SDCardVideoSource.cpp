#include <Arduino.h>
#include "SDCardVideoSource.h"
#include "AVIParser.h"
#include "../SDCard.h"

SDCardVideoSource::SDCardVideoSource(SDCard *sdCard, const char *aviPath) : mSDCard(sdCard), mAviPath(aviPath)
{
}

void SDCardVideoSource::start()
{
    // nothing to do!
}

bool SDCardVideoSource::fetchChannelData()
{
    // check the the sd card is mounted
    if (!mSDCard->isMounted())
    {
        Serial.println("SD card is not mounted");
        return false;
    }
    // get the list of AVI files
    mAviFiles = mSDCard->listFiles(mAviPath, ".avi");
    if (mAviFiles.size() == 0)
    {
        Serial.println("No AVI files found");
        return false;
    }
    return true;
}

void SDCardVideoSource::setChannel(int channel)
{
    mFrameCount = 0;
    if (!mSDCard->isMounted())
    {
        Serial.println("SD card is not mounted");
        return;
    }
    // check that the channel is valid
    if (channel < 0 || channel >= mAviFiles.size())
    {
        Serial.printf("Invalid channel %d\n", channel);
        return;
    }
    // close any open AVI files
    // if (mCurrentChannelAudioParser)
    // {
    //     delete mCurrentChannelAudioParser;
    //     mCurrentChannelAudioParser = NULL;
    // }
    if (mCurrentChannelVideoParser)
    {
        delete mCurrentChannelVideoParser;
        mCurrentChannelVideoParser = NULL;
    }
    // open the AVI file
    std::string aviFilename = mAviFiles[channel];
    Serial.printf("Opening AVI file %s\n", aviFilename.c_str());
    // mCurrentChannelAudioParser = new AVIParser(aviFilename, AVIChunkType::AUDIO);
    // if (!mCurrentChannelAudioParser->open())
    // {
    //     Serial.printf("Failed to open AVI file %s\n", aviFilename.c_str());
    //     delete mCurrentChannelAudioParser;
    //     mCurrentChannelAudioParser = NULL;
    // }
    mCurrentChannelVideoParser = new AVIParser(aviFilename, AVIChunkType::VIDEO);
    if (!mCurrentChannelVideoParser->open())
    {
        Serial.printf("Failed to open AVI file %s\n", aviFilename.c_str());
        delete mCurrentChannelVideoParser;
        mCurrentChannelVideoParser = NULL;
        // delete mCurrentChannelAudioParser;
        // mCurrentChannelAudioParser = NULL;
    }
    mChannelNumber = channel;
}

void SDCardVideoSource::nextChannel()
{
    int channel = mChannelNumber + 1;
    if (channel >= mAviFiles.size())
    {
        channel = 0;
    }
    setChannel(channel);
}

bool SDCardVideoSource::getVideoFrame(uint8_t **buffer, size_t &bufferLength, size_t &frameLength)
{
    if (!mCurrentChannelVideoParser)
    {
        return false;
    }
    if (mState == VideoPlayerState::STOPPED || mState == VideoPlayerState::STATIC)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        return false;
    }
    if (mState == VideoPlayerState::PAUSED)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        return false;
    }
    // how long should we wait before fetching the next frame?
    float frameRate = mCurrentChannelVideoParser->getFrameRate();
    if (frameRate > 0)
    {
        float frameTime = 1000.0f / frameRate;
        long delay = frameTime - (millis() - mLastFrameTime);
        if (delay > 0)
        {
            vTaskDelay(delay / portTICK_PERIOD_MS);
        }
    }
    mLastFrameTime = millis();
    frameLength = mCurrentChannelVideoParser->getNextChunk((uint8_t **)buffer, bufferLength);
    if (frameLength == 0)
    {
        // end of video, move to next one
        nextChannel();
        return false;
    }
    mFrameCount++;
    return true;
}

std::string SDCardVideoSource::getChannelName()
{
    if (mChannelNumber >= 0 && mChannelNumber < mAviFiles.size())
    {
        // we just want the filename, not the full path
        std::string fullPath = mAviFiles[mChannelNumber];
        size_t lastSlash = fullPath.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }
    return "Unknown";
}
