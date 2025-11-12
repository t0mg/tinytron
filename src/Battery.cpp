#include "Battery.h"
#include <Arduino.h>

Battery::Battery(int pin, float vRef, float r1, float r2)
    : _pin(pin), _vRef(vRef), _r1(r1), _r2(r2) {
}

void Battery::begin() {
    pinMode(_pin, INPUT);
}

float Battery::getVoltage() {
    int adcValue = analogRead(_pin);
    float voltage = (float)adcValue * (_vRef / 4095.0);
    float actualVoltage = voltage * ((_r1 + _r2) / _r2);
    return actualVoltage;
}
