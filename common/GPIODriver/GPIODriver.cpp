#include <unistd.h>
#include "GPIODriver.hpp"
#include "Arduino.h"

GPIODriver::GPIODriver(uint32_t pin)
{
    this->mask = 1 << pin;
    this->pin = pin;
};

void GPIODriver::init()
{
    *GPIO_ENABLE_REGISTER = *GPIO_ENABLE_REGISTER | mask;
}

void GPIODriver::initInput()
{
    pinMode(pin, INPUT);
};

bool GPIODriver::read()
{
    return (*GPIO_IN_REGISTER >> pin) & 1;
}

void GPIODriver::setHigh()
{
    *GPIO_OUT_REGISTER |= this->mask;
}

void GPIODriver::setLow()
{
    *GPIO_OUT_REGISTER &= ~this->mask;
}
