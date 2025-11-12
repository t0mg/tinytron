#ifndef BATTERY_H
#define BATTERY_H

class Battery {
public:
    Battery(int pin, float vRef, float r1, float r2);
    void begin();
    float getVoltage();

private:
    int _pin;
    float _vRef;
    float _r1;
    float _r2;
};

#endif // BATTERY_H
