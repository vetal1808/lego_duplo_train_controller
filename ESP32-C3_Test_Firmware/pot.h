#ifndef POT_H
#define POT_H

#include <stdint.h>

class Pot
{
private:
    int pin_name;
    uint16_t offset;
    uint16_t gain;
    int16_t lastValue;
    int16_t lastEventValue;
    bool eventTriggered;
    uint16_t eventThreshold;
    void (*onChangeCallback)();
    uint16_t readAnalogValue();

public:
    Pot();
    Pot(int pinName, uint16_t gain, uint16_t offset, uint16_t eventThreshold);
    int16_t update();
    int16_t get();
    bool isEventTriggered();
    void setOnChange(void (*callback)());
};

#endif
