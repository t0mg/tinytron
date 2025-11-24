#include "Button.h"

Button::Button(int pin, int sys_en_pin)
{
  _pin = pin;
  _sys_en_pin = sys_en_pin;
  pinMode(_pin, INPUT_PULLUP);
  pinMode(_sys_en_pin, OUTPUT);
  digitalWrite(_sys_en_pin, HIGH);

  debounceDelay = 50;
  clickInterval = 500;
  longPressDuration = 1000;
  reset();
}

void Button::reset()
{
  longPressDetected = false;
  clickDetected = false;
  doubleClickDetected = false;
  clickCount = 0;
  // read initial state
  lastButtonState = digitalRead(_pin);
  buttonState = lastButtonState;
  // important to avoid false triggers
  lastClickTime = millis();
  lastDebounceTime = millis();
}


void Button::update()
{
  clickDetected = false;
  doubleClickDetected = false;

  int reading = digitalRead(_pin);

  if (reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    if (reading != buttonState)
    {
      buttonState = reading;

      if (buttonState == LOW)
      { // Button pressed
        lastClickTime = millis();
      }
      else
      { // Button released
        if (!longPressDetected)
        {
          unsigned long now = millis();
          if (now - lastClickTime < clickInterval)
          {
            clickCount++;
            if (clickCount == 1)
            {
              // Start of a potential double click
            }
          }
        }
        longPressDetected = false;
      }
    }
  }

  if (buttonState == LOW && !longPressDetected && (millis() - lastClickTime) > longPressDuration)
  {
    Serial.println("Long Press");
    longPressDetected = true;
    digitalWrite(_sys_en_pin, LOW);
  }

  if (clickCount > 0 && (millis() - lastClickTime) >= clickInterval)
  {
    if (clickCount == 1)
    {
      clickDetected = true;
      Serial.println("Single Click");
    }
    else if (clickCount == 2)
    {
      doubleClickDetected = true;
      Serial.println("Double Click");
    }
    clickCount = 0;
  }

  lastButtonState = reading;
}

bool Button::isClicked()
{
  return clickDetected;
}

bool Button::isDoubleClicked()
{
  return doubleClickDetected;
}

void Button::powerOff()
{
  digitalWrite(_sys_en_pin, LOW);
}
