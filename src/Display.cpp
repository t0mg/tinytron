#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Display.h"

// PWM channel for backlight
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_8_BIT 8
#define LEDC_BASE_FREQ 5000

Display::Display(Prefs *prefs) : tft(new TFT_eSPI()), _prefs(prefs)
{
  tft_mutex = xSemaphoreCreateRecursiveMutex();

  // First, initialize the TFT itself and set the rotation
  tft->init();
#ifdef CHEAP_YELLOW_DISPLAY
  tft->setRotation(1);
#else
  tft->setRotation(3);
#endif

#ifdef BOARD_HAS_PSRAM
  // Now create the sprite with the correct, rotated dimensions
  frameSprite = new TFT_eSprite(tft);
  frameSprite->createSprite(tft->width(), tft->height());
  frameSprite->setTextFont(2);
  frameSprite->setTextSize(2);
#endif

// setup the backlight
#ifdef TFT_BL
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
  ledcAttachPin(TFT_BL, LEDC_CHANNEL_0);
  ledcWrite(LEDC_CHANNEL_0, 255); // turn on backlight
#endif
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  tft->fillScreen(TFT_BLACK);
#ifdef USE_DMA
  tft->initDMA();
#endif
  tft->fillScreen(TFT_BLACK);
  tft->setTextFont(2);
  tft->setTextSize(2);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  xSemaphoreGiveRecursive(tft_mutex);
}

void Display::setBrightness(uint8_t brightness)
{
#ifdef TFT_BL
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  ledcWrite(LEDC_CHANNEL_0, brightness);
  xSemaphoreGiveRecursive(tft_mutex);
#endif
}

// this function now draws directly to the screen, used for non-buffered drawing
void Display::drawPixels(int x, int y, int width, int height, uint16_t *pixels)
{
  int numPixels = width * height;
  if (dmaBuffer[dmaBufferIndex] == NULL)
  {
    dmaBuffer[dmaBufferIndex] = (uint16_t *)malloc(numPixels * 2);
  }
  memcpy(dmaBuffer[dmaBufferIndex], pixels, numPixels * 2);
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  tft->setAddrWindow(x, y, width, height);
#ifdef USE_DMA
  tft->pushPixelsDMA(dmaBuffer[dmaBufferIndex], numPixels);
#else
  tft->pushPixels(dmaBuffer[dmaBufferIndex], numPixels);
#endif
  xSemaphoreGiveRecursive(tft_mutex);
  dmaBufferIndex = (dmaBufferIndex + 1) % 2;
}

// new function to draw to our framebuffer sprite
void Display::drawPixelsToSprite(int x, int y, int width, int height, uint16_t *pixels)
{
#ifdef BOARD_HAS_PSRAM
  frameSprite->pushImage(x, y, width, height, pixels);
#else
  drawPixels(x, y, width, height, pixels);
#endif
}

// new function to push the framebuffer to the screen
void Display::flushSprite()
{
#ifdef BOARD_HAS_PSRAM
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  frameSprite->pushSprite(0, 0);
  xSemaphoreGiveRecursive(tft_mutex);
#endif
}

void Display::fillSprite(uint16_t color)
{
#ifdef BOARD_HAS_PSRAM
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  frameSprite->fillSprite(color);
  xSemaphoreGiveRecursive(tft_mutex);
#else
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  tft->fillScreen(color);
  xSemaphoreGiveRecursive(tft_mutex);
#endif
}

int Display::width()
{
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  int w = tft->width();
  xSemaphoreGiveRecursive(tft_mutex);
  return w;
}

int Display::height()
{
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  int h = tft->height();
  xSemaphoreGiveRecursive(tft_mutex);
  return h;
}

void Display::fillScreen(uint16_t color)
{
#ifdef BOARD_HAS_PSRAM
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  frameSprite->fillScreen(color);
  xSemaphoreGiveRecursive(tft_mutex);
#else
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  tft->fillScreen(color);
  xSemaphoreGiveRecursive(tft_mutex);
#endif
}

void Display::drawOSD(const char *text, OSDPosition position, OSDLevel level)
{
  if (_prefs->getOsdLevel() < level)
  {
    return;
  }
#ifdef BOARD_HAS_PSRAM
  // draw OSD text into the sprite, with a black background for readability
  frameSprite->setTextColor(TFT_ORANGE, TFT_BLACK);

  int textWidth = frameSprite->textWidth(text);
  int textHeight = frameSprite->fontHeight();
  int x = 0;
  int y = 0;

  switch (position)
  {
  case TOP_LEFT:
    x = 20;
    y = 20;
    break;
  case TOP_RIGHT:
    x = width() - textWidth - 20;
    y = 20;
    break;
  case BOTTOM_LEFT:
    x = 20;
    y = height() - textHeight - 20;
    break;
  case BOTTOM_RIGHT:
    x = width() - textWidth - 20;
    y = height() - textHeight - 20;
    break;
  case CENTER:
    x = (width() - textWidth) / 2;
    y = (height() - textHeight) / 2;
    break;
  }
  frameSprite->setCursor(x, y);
  frameSprite->println(text);
#else
  // no sprite, draw directly to the screen
  xSemaphoreTakeRecursive(tft_mutex, portMAX_DELAY);
  tft->setTextColor(TFT_ORANGE, TFT_BLACK);

  int textWidth = tft->textWidth(text);
  int textHeight = tft->fontHeight();
  int x = 0;
  int y = 0;

  switch (position)
  {
  case TOP_LEFT:
    x = 20;
    y = 20;
    break;
  case TOP_RIGHT:
    x = width() - textWidth - 20;
    y = 20;
    break;
  case BOTTOM_LEFT:
    x = 20;
    y = height() - textHeight - 20;
    break;
  case BOTTOM_RIGHT:
    x = width() - textWidth - 20;
    y = height() - textHeight - 20;
    break;
  case CENTER:
    x = (width() - textWidth) / 2;
    y = (height() - textHeight) / 2;
    break;
  }
  tft->setCursor(x, y);
  tft->println(text);
  xSemaphoreGiveRecursive(tft_mutex);
#endif
}


