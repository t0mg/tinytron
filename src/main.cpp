#include <Arduino.h>
#include "Button.h"
#include "Display.h"
#include "VideoPlayer/StreamVideoSource.h"
#include "VideoPlayer/SDCardVideoSource.h"
#include "VideoPlayer/VideoPlayer.h"
#include "VideoPlayer/AVIParser.h"
#include "SDCard.h"
#include <Wire.h>
#include "Prefs.h"
#include "WifiManager.h"
#include "Battery.h"
#include <ESPAsyncWebServer.h>

#ifndef USE_DMA
#warning "No DMA - Drawing may be slower"
#endif

VideoSource *videoSource = NULL;
VideoPlayer *videoPlayer = NULL;
Display display;
Button button(SYS_OUT, SYS_EN);
Prefs prefs;
AsyncWebServer server(80);
Battery battery(BATTERY_VOLTAGE_PIN, 3.3, 200000.0, 100000.0);
WifiManager wifiManager(&server, &prefs, &battery);
bool wifiManagerActive = false;

void setup()
{
  display.fillScreen(TFT_BLACK);
  Serial.begin(115200);
  delay(500); // Wait for serial to initialize

  battery.begin();
  prefs.begin();
  prefs.onBrightnessChanged([](int brightness)
                            { display.setBrightness(brightness); });
  display.setBrightness(prefs.getBrightness());
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

  Serial.println("Looking for SD Card");
  SDCard *card = new SDCard(SD_CARD_MISO, SD_CARD_MOSI, SD_CARD_CLK, SD_CARD_CS);
  // check that the SD Card has mounted properly
  if (!card->isMounted())
  {
    display.fillScreen(TFT_BLACK);
    display.drawText("No SD Card", "Initializing WiFi...");
    Serial.println("Failed to mount SD Card. Initializing WifiManager.");
    wifiManager.begin();
    wifiManagerActive = true;
    Serial.println("");
    Serial.println("WiFi connected");
    // TODO: Display IP address on screen. For AP mode, display SSID and preset IP.
    Serial.printf("Wifi Connected: %s\n", wifiManager.getIpAddress().toString().c_str());
    display.fillScreen(TFT_BLACK);
    display.drawText(wifiManager.isAPMode() ? "AP: " AP_SSID :  "WiFi Connected", wifiManager.getIpAddress().toString().c_str());
    if (!wifiManager.isAPMode()) {
      videoSource = new StreamVideoSource(&server);
    }
  }
  else
  {
    Serial.println("SD Card mounted successfully.");
    display.drawText("SD Card found !");
    videoSource = new SDCardVideoSource(card, "/");
  }

  if (videoSource != nullptr)
  {
    videoPlayer = new VideoPlayer(
        videoSource,
        display);
    videoPlayer->start();
    // get the channel info
    while (!videoSource->fetchChannelData())
    {
      Serial.println("Failed to fetch channel data");
      delay(1000);
    }
    // default to first channel
    videoPlayer->setChannel(0);
    delay(500);
    videoPlayer->play();
  }
  // reset the button state
  button.reset();
}

void loop()
{
  delay(5);

  button.update();
  if (wifiManagerActive)
  {
    wifiManager.handleClient();
  }

  if (!wifiManagerActive)
  {
    if (button.isClicked())
    {
      if (videoPlayer != nullptr)
      {
        videoPlayer->playStatic();
        delay(500);
        videoPlayer->play();
      }
    }

    if (button.isDoubleClicked())
    {
      // Handle double click
    }
  }
}
