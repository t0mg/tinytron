#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>

class StatusLED {
public:
    StatusLED(int pin, int channel = 1);
    void begin();
    void update();

    /**
     * Set a breathing/blinking pattern.
     * @param off_ms Duration the LED is completely off.
     * @param fadeIn_ms Duration of the fade-in (from 0 to 255).
     * @param on_ms Duration the LED is completely on at full brightness.
     * @param fadeOut_ms Duration of the fade-out (from 255 to 0). If -1, defaults to fadeIn_ms.
     */
    void setPattern(int off_ms, int fadeIn_ms, int on_ms, int fadeOut_ms = -1);

    void off();
    void solid(uint8_t brightness = 255);
    void breathe(int duration = 2000);
    void blink(int duration = 500);
    void fastBlink(int duration = 100);

private:
    int _pin;
    int _channel;

    int _off_ms;
    int _fadeIn_ms;
    int _on_ms;
    int _fadeOut_ms;

    int _totalCycle_ms;
    uint8_t _maxBrightness;
};

#endif // STATUS_LED_H
