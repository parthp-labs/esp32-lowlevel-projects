#include <unistd.h>

class GPIODriver
{
private:
    volatile uint32_t *GPIO_ENABLE_REGISTER = (volatile uint32_t *)0x3ff44020;
    volatile uint32_t *GPIO_OUT_REGISTER = (volatile uint32_t *)0x3ff44004;
    volatile uint32_t *GPIO_IN_REGISTER = (volatile uint32_t *)0x3FF4403C;
    uint32_t pin = 0;
    uint32_t mask = 0;

public:
    GPIODriver(uint32_t pin);
    void init();
    void setHigh();
    void setLow();
    void initInput();
    bool read();
};