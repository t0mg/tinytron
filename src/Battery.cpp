#include "Battery.h"
#include <Arduino.h>

// Voltage (mV) to Percentage mapping
const int voltage_map[][2] = {
    {3780, 100}, {3600, 80}, {3400, 60}, {3200, 40}, {3000, 20},
    {2800, 10},  {2600, 5},  {2550, 1}
};

Battery::Battery(int pin, float vRef, float r1, float r2)
    : _pin(pin), _vRef(vRef), _r1(r1), _r2(r2),
      _voltage(0), _battery_level(0), _is_charging(false), _is_low_battery(false),
      _last_voltage(0), _last_update_time(0) {}

void Battery::begin() {
    pinMode(_pin, INPUT);
    update(); // Initial reading
    _last_voltage = _voltage;
}

void Battery::update() {
    int adcValue = analogRead(_pin);
    float voltage = (float)adcValue * (_vRef / 4095.0);
    _voltage = voltage * ((_r1 + _r2) / _r2);

    // Battery level calculation
    const int num_voltage_map_entries = sizeof(voltage_map) / sizeof(voltage_map[0]);
    int millivolts = _voltage * 1000;
    if (millivolts >= voltage_map[0][0]) {
        _battery_level = 100;
    } else if (millivolts <= voltage_map[num_voltage_map_entries - 1][0]) {
        _battery_level = 1;
    } else {
        int i = 0;
        while (millivolts < voltage_map[i][0]) {
            i++;
        }
        int upper_voltage = voltage_map[i - 1][0];
        int lower_voltage = voltage_map[i][0];
        int upper_percentage = voltage_map[i - 1][1];
        int lower_percentage = voltage_map[i][1];
        _battery_level = lower_percentage + (float)(millivolts - lower_voltage) / (upper_voltage - lower_voltage) * (upper_percentage - lower_percentage);
    }
    
    // Low battery flag
    _is_low_battery = (_battery_level <= 10);

    // Charging detection
    uint32_t now = millis();
    if (now - _last_update_time > 5000) { // Check every 5 seconds
        if (_voltage > _last_voltage + 0.05) { // Voltage increased by 50mV
            _is_charging = true;
        } else if (_voltage < _last_voltage - 0.01) { // Voltage is dropping, so not charging
            _is_charging = false;
        }
        _last_voltage = _voltage;
        _last_update_time = now;
    }
}

float Battery::getVoltage() {
    return _voltage;
}

int Battery::getBatteryLevel() {
    return _battery_level;
}

bool Battery::isCharging() {
    return _is_charging;
}

bool Battery::isLowBattery() {
    return _is_low_battery;
}
