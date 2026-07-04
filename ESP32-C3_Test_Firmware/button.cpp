#include "button.h"
#include <Arduino.h>

Button::Button()
{
    pin_name = -1; // Invalid pin
    pin_state_sequence = 0;
    rise_detected = false;
    fall_detected = false;
    state = false;
    onRiseCallback = nullptr;
    onFallCallback = nullptr;
}

Button::Button(int pinName)
{
    pin_name = pinName;
    pinMode(pin_name, INPUT_PULLUP);
    pin_state_sequence = 0;
    rise_detected = false;
    fall_detected = false;
    state = false;
    onRiseCallback = nullptr;
    onFallCallback = nullptr;
}

uint8_t Button::readPinState()
{
    return digitalRead(pin_name) ? 1 : 0;
}

void Button::update()
{
    uint8_t currentState = readPinState();
    pin_state_sequence = (pin_state_sequence << 1) | currentState;
    if (pin_state_sequence == 0x7f)
    {
        rise_detected = true;
        if (onRiseCallback)
            onRiseCallback();
    }
    if (pin_state_sequence == 0x80)
    {
        fall_detected = true;
        if (onFallCallback)
            onFallCallback();
    }
    if (pin_state_sequence == 0x00)
    {
        rise_detected = false;
        fall_detected = false;
    }
    if (pin_state_sequence == 0xff)
    {
        state = true;
    }
    else if (pin_state_sequence == 0x00)
    {
        state = false;
    }
}

bool Button::getState()
{
    return state;
}

bool Button::isRiseDetected()
{
    bool detected = rise_detected;
    rise_detected = false;
    return detected;
}

bool Button::isFallDetected()
{
    bool detected = fall_detected;
    fall_detected = false;
    return detected;
}

void Button::setOnRise(void (*callback)())
{
    onRiseCallback = callback;
}

void Button::setOnFall(void (*callback)())
{
    onFallCallback = callback;
}
