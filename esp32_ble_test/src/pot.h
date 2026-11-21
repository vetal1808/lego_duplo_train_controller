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
    uint16_t lastRaw;
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
    uint16_t getRaw(); // last ADC reading, for calibration
    bool isEventTriggered();
    void setOnChange(void (*callback)());
};

#endif
