#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

class Button
{
private:
    int pin_name;
    uint8_t pin_state_sequence;
    bool rise_detected;
    bool fall_detected;
    bool state;
    void (*onRiseCallback)();
    void (*onFallCallback)();
    uint8_t readPinState();

public:
    Button();
    Button(int pinName);
    void update();
    bool getState();
    bool isRiseDetected();
    bool isFallDetected();
    void setOnRise(void (*callback)());
    void setOnFall(void (*callback)());
};

#endif
