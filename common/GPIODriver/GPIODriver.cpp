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

void GPIODriver::setHigh()
{
    *GPIO_OUT_REGISTER |= this->mask;
}

void GPIODriver::setLow()
{
    *GPIO_OUT_REGISTER &= ~this->mask;
}
