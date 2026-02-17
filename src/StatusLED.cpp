#include "StatusLED.h"

#define LEDC_TIMER_8_BIT 8
#define LEDC_BASE_FREQ 5000

StatusLED::StatusLED(int pin, int channel)
    : _pin(pin), _channel(channel), _off_ms(0), _fadeIn_ms(0), _on_ms(0), _fadeOut_ms(0), _totalCycle_ms(0), _maxBrightness(255) {
}

void StatusLED::begin() {
    if (_pin < 0) return;

    // Check if the pin is a valid GPIO
    if (!digitalPinIsValid(_pin)) return;

    ledcSetup(_channel, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
    ledcAttachPin(_pin, _channel);
    ledcWrite(_channel, 0);
}

void StatusLED::setPattern(int off_ms, int fadeIn_ms, int on_ms, int fadeOut_ms) {
    _off_ms = off_ms;
    _fadeIn_ms = fadeIn_ms;
    _on_ms = on_ms;
    _fadeOut_ms = (fadeOut_ms < 0) ? fadeIn_ms : fadeOut_ms;
    _totalCycle_ms = _off_ms + _fadeIn_ms + _on_ms + _fadeOut_ms;
    _maxBrightness = 255;

    if (_totalCycle_ms == 0) {
        // If all are zero, it's effectively off, but we need a cycle for update()
        _totalCycle_ms = 1;
    }
}

void StatusLED::update() {
    if (_pin < 0 || _totalCycle_ms <= 0) return;

    unsigned long now = millis();
    int timeInCycle = now % _totalCycle_ms;
    int brightness = 0;

    if (timeInCycle < _off_ms) {
        // Off phase
        brightness = 0;
    } else if (timeInCycle < _off_ms + _fadeIn_ms) {
        // Fade in phase
        int fadeTime = timeInCycle - _off_ms;
        brightness = (fadeTime * _maxBrightness) / _fadeIn_ms;
    } else if (timeInCycle < _off_ms + _fadeIn_ms + _on_ms) {
        // On phase
        brightness = _maxBrightness;
    } else {
        // Fade out phase
        int fadeTime = timeInCycle - (_off_ms + _fadeIn_ms + _on_ms);
        if (_fadeOut_ms > 0) {
            brightness = _maxBrightness - (fadeTime * _maxBrightness) / _fadeOut_ms;
        } else {
            brightness = 0;
        }
    }

    ledcWrite(_channel, brightness);
}

void StatusLED::off() {
    setPattern(1000, 0, 0, 0);
    ledcWrite(_channel, 0);
}

void StatusLED::solid(uint8_t brightness) {
    // A trick to make it solid: make it all "on" phase
    setPattern(0, 0, 1000, 0);
    _maxBrightness = brightness;
}

void StatusLED::breathe(int duration) {
    int fade = duration / 2;
    setPattern(0, fade, 0, fade);
}

void StatusLED::blink(int duration) {
    setPattern(duration, 0, duration, 0);
}

void StatusLED::fastBlink(int duration) {
    setPattern(duration, 0, duration, 0);
}
