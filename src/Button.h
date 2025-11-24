#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

class Button
{
public:
  Button(int pin, int sys_en_pin);
  void update();
  void reset();
  bool isClicked();
  bool isDoubleClicked();
  void powerOff();

private:
  int _pin;
  int _sys_en_pin;

  bool buttonState;
  bool lastButtonState;
  unsigned long lastDebounceTime;
  unsigned long debounceDelay;
  unsigned long lastClickTime;
  unsigned long clickInterval;
  unsigned long longPressDuration;

  bool longPressDetected;
  bool clickDetected;
  bool doubleClickDetected;

  int clickCount;
};

#endif // BUTTON_H
