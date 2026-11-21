#include "pot.h"
#include <Arduino.h>

#define SCALE 65536

Pot::Pot()
{
    pin_name = -1; // Invalid pin
    gain = 0;
    offset = 0;
    eventThreshold = 0;
    lastValue = 0;
    lastRaw = 0;
    lastEventValue = 0;
    eventTriggered = false;
    onChangeCallback = nullptr;
}

Pot::Pot(int pinName, uint16_t gain, uint16_t offset, uint16_t eventThreshold)
{
    pin_name = pinName;
    this->gain = gain;
    this->offset = offset;
    this->eventThreshold = eventThreshold;
    lastValue = 0;
    lastRaw = 0;
    lastEventValue = 0;
    eventTriggered = false;
    onChangeCallback = nullptr; // was left uninitialised before: a call through a garbage pointer
    pinMode(pin_name, INPUT);
}

uint16_t Pot::readAnalogValue()
{
    if (pin_name < 0)
    {
        return 0;
    }
    uint16_t value = analogRead(pin_name);
    return value;
}

int16_t Pot::update()
{
    uint16_t currentValue = readAnalogValue();
    lastRaw = currentValue;
    int16_t normalizedValue = ((int32_t)((int32_t)currentValue - offset) * gain) / SCALE;
    lastValue = normalizedValue;

    if (abs(normalizedValue - lastEventValue) >= eventThreshold)
    {
        lastEventValue = normalizedValue;
        eventTriggered = true;
        if (onChangeCallback)
        {
            onChangeCallback();
        }
    }
    else
    {
        eventTriggered = false;
    }

    return normalizedValue;
}

int16_t Pot::get()
{
    return lastValue;
}

uint16_t Pot::getRaw()
{
    return lastRaw;
}

bool Pot::isEventTriggered()
{
    return eventTriggered;
}

void Pot::setOnChange(void (*callback)())
{
    onChangeCallback = callback;
}