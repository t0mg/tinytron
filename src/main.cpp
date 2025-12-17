#include "Battery.h"
#include "Button.h"
#include "Display.h"
#include "ImagePlayer/ImagePlayer.h"
#include "ImagePlayer/SDCardImageSource.h"
#include "Prefs.h"
#include "SDCard.h"
#include "VideoPlayer/AVIParser.h"
#include "VideoPlayer/SDCardVideoSource.h"
#include "VideoPlayer/StreamVideoSource.h"
#include "VideoPlayer/VideoPlayer.h"
#include "WifiManager.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>

#ifndef USE_DMA
#warning "No DMA - Drawing may be slower"
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

VideoSource *videoSource = NULL;
ImageSource *imageSource = NULL;

MediaPlayer *videoPlayer = NULL;
MediaPlayer *imagePlayer = NULL;
MediaPlayer *currentPlayer = NULL;

Prefs prefs;
Display display(&prefs);
Button button(SYS_OUT, SYS_EN);
AsyncWebServer server(80);
Battery battery(BATTERY_VOLTAGE_PIN, 3.3, 200000.0, 100000.0);
unsigned long shutdown_time = 0;
WifiManager wifiManager(&server, &prefs, &battery);
bool wifiManagerActive = false;

enum class PlaybackMode
{
  VIDEO_ONLY,
  IMAGE_ONLY,
  VIDEO_THEN_IMAGES
};
PlaybackMode playbackMode = PlaybackMode::VIDEO_ONLY;

void setShutdownTime(int minutes)
{
  if (minutes > 0)
  {
    shutdown_time = millis() + minutes * 60 * 1000;
    Serial.printf("Timer set, shutting down in %d minutes\n", minutes);
  }
  else
  {
    shutdown_time = 0;
    Serial.println("Timer disabled");
  }
}

void setup()
{
  display.fillScreen(TFT_BLACK);
  Serial.begin(115200);
  delay(500); // Wait for serial to initialize

  battery.begin();
  prefs.begin();
  prefs.onBrightnessChanged(
      [](int brightness)
      { display.setBrightness(brightness); });
  prefs.onTimerMinutesChanged([](int minutes)
                              { setShutdownTime(minutes); });
  setShutdownTime(prefs.getTimerMinutes());
  display.setBrightness(prefs.getBrightness());
  display.drawOSD("Tinytron", CENTER, STANDARD);
  display.drawOSD(TOSTRING(APP_VERSION) " " TOSTRING(APP_BUILD_NUMBER),
                  BOTTOM_RIGHT, DEBUG);
  display.flushSprite();
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  Serial.println("Looking for SD Card");
  SDCard *card =
      new SDCard(SD_CARD_MISO, SD_CARD_MOSI, SD_CARD_CLK, SD_CARD_CS);
  // check that the SD Card has mounted properly
  if (!card->isMounted())
  {
    display.fillScreen(TFT_BLACK);
    display.drawOSD("No SD Card", TOP_LEFT, STANDARD);
    display.drawOSD("Initializing WiFi...", CENTER, STANDARD);
    display.flushSprite();
    Serial.println("Failed to mount SD Card. Initializing WifiManager.");
    wifiManager.begin();
    wifiManagerActive = true;
    Serial.printf("Wifi Connected: %s\n",
                  wifiManager.getIpAddress().toString().c_str());
    display.fillScreen(TFT_BLACK);
    display.drawOSD(wifiManager.isAPMode() ? "AP: " AP_SSID : "WiFi Connected",
                    TOP_LEFT, STANDARD);
    display.drawOSD(wifiManager.getIpAddress().toString().c_str(), CENTER,
                    STANDARD);
    display.flushSprite();
    if (!wifiManager.isAPMode())
    {
      videoSource = new StreamVideoSource(&server);
    }
  }
  else
  {
    display.fillScreen(TFT_BLACK);
    Serial.println("SD Card mounted successfully.");
    display.drawOSD("SD Card found !", CENTER, STANDARD);
    display.flushSprite();

    VideoSource *videoCandidate = new SDCardVideoSource(card, "/");
    if (videoCandidate->fetchVideoData())
    {
      videoSource = videoCandidate;
    }
    else
    {
      delete videoCandidate;
    }

    ImageSource *imageCandidate = new SDCardImageSource(card, "/", false);
    if (imageCandidate->fetchImageData())
    {
      imageSource = imageCandidate;
    }
    else
    {
      delete imageCandidate;
    }
  }

  if (videoSource != nullptr)
  {
    videoPlayer = new VideoPlayer(videoSource, display, prefs, battery);
    videoPlayer->start();
    while (!videoSource->fetchVideoData())
    {
      Serial.println("Failed to fetch video data");
      delay(1000);
    }
    videoPlayer->set(0);
    delay(500);
  }

  if (imageSource != nullptr)
  {
    imagePlayer = new ImagePlayer(imageSource, display, prefs, battery);
    imagePlayer->start();
    while (!imageSource->fetchImageData())
    {
      Serial.println("Failed to fetch image data");
      delay(1000);
    }
    imagePlayer->set(0);
    delay(500);
  }

  if (videoSource != nullptr && imageSource != nullptr)
  {
    playbackMode = PlaybackMode::VIDEO_THEN_IMAGES;
    currentPlayer = videoPlayer;
  }
  else if (videoSource != nullptr)
  {
    playbackMode = PlaybackMode::VIDEO_ONLY;
    currentPlayer = videoPlayer;
  }
  else if (imageSource != nullptr)
  {
    playbackMode = PlaybackMode::IMAGE_ONLY;
    currentPlayer = imagePlayer;
  }

  if (currentPlayer)
  {
    currentPlayer->play();
  }

  // reset the button state
  button.reset();
}

unsigned long lastBatteryUpdate = 0;

void loop()
{
  delay(5);

  unsigned long now = millis();
  if (shutdown_time > 0 && now > shutdown_time)
  {
    if (videoPlayer != nullptr)
    {
      videoPlayer->stop();
    }
    if (imagePlayer != nullptr)
    {
      imagePlayer->stop();
    }
    display.fillScreen(TFT_BLACK);
    display.drawOSD("Time out!", CENTER, STANDARD);
    display.flushSprite();
    delay(5000);
    button.powerOff();
  }
  if (now - lastBatteryUpdate > 10000)
  {
    battery.update();
    lastBatteryUpdate = now;
  }

  button.update();
  if (wifiManagerActive)
  {
    wifiManager.handleClient();
  }

  if (!wifiManagerActive)
  {
    if (playbackMode == PlaybackMode::VIDEO_THEN_IMAGES)
    {
      if (currentPlayer == videoPlayer)
      {
        SDCardVideoSource *sdVideoSource = (SDCardVideoSource *)videoSource;
        if (sdVideoSource != nullptr && sdVideoSource->consumeWrapped())
        {
          videoPlayer->stop();
          currentPlayer = imagePlayer;
          SDCardImageSource *sdImageSource = (SDCardImageSource *)imageSource;
          if (sdImageSource != nullptr)
          {
            sdImageSource->consumeWrapped(); // Prevent bounce-back
            imagePlayer->set(0);
          }
          delay(50);
          currentPlayer->play();
        }
      }
      else // currentPlayer == imagePlayer
      {
        SDCardImageSource *sdImageSource = (SDCardImageSource *)imageSource;
        if (sdImageSource != nullptr && sdImageSource->consumeWrapped())
        {
          imagePlayer->stop();
          currentPlayer = videoPlayer;
          SDCardVideoSource *sdVideoSource = (SDCardVideoSource *)videoSource;
          if (sdVideoSource != nullptr)
          {
            sdVideoSource->consumeWrapped(); // Prevent bounce-back
            videoPlayer->set(0);
          }
          delay(50);
          currentPlayer->play();
        }
      }
    }

    if (button.isClicked())
    {
      if (currentPlayer != nullptr)
      {
        currentPlayer->playPauseToggle();
      }
    }

    if (button.isDoubleClicked())
    {
      if (currentPlayer != nullptr)
      {
        currentPlayer->next();
      }
    }
  }
}