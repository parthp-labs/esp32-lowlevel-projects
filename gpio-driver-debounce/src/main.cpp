#include <Arduino.h>
#include "GPIODriver.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Our own register-level GPIO driver (bypasses digitalWrite/pinMode for the LED)
GPIODriver led(2);

SemaphoreHandle_t buttonSemaphore;

// Tracks which blink pattern we're currently on (0, 1, or 2)
int pattern = 0;

// Timestamp (in ms) of the last ISR trigger accepted. Used for handling debounce from the push button
volatile uint32_t lastInterruptTime = 0;

// ISR: fires on every FALLING edge on the button pin.
// IRAM_ATTR forces this function's code into internal RAM instead of flash,
// so it's still executable even if flash access is briefly unavailable.
void IRAM_ATTR buttonInterrupt()
{
  uint32_t now = millis();

  // Handling the debouncing from the push button. if the next interrupt is within 200 ms, ignore it as its a noise from the push button in the form of debouncing
  if (now - lastInterruptTime < 200)
    return;
  // Reset the last interrupt time to current time
  lastInterruptTime = now;

  // Will be set to pdTRUE by xSemaphoreGiveFromISR if giving this semaphore
  // just unblocked a task with HIGHER priority than whatever the CPU
  // was running when this interrupt fired.
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  // Signal the semaphore — this is the actual "hand-off" to ledTask.
  xSemaphoreGiveFromISR(buttonSemaphore, &higherPriorityTaskWoken);

  // If a higher-priority task just became ready (ledTask waking up),
  // force an immediate context switch to it now, rather than waiting
  // for the next scheduler tick. Must be the last thing the ISR does.
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

// FreeRTOS task to handle the led pattern
void ledTask(void *parameter)
{
  int delayTime = 1000; // current blink half-period, in ms

  while (1)
  {
    // Non-blocking check (timeout = 0): "has the ISR signaled a press
    // since I last checked?" If yes, this returns pdTRUE and consumes
    // the signal (takes the semaphore).
    // If no signal is pending, this returns immediately without blocking, so blinking never stalls.
    if (xSemaphoreTake(buttonSemaphore, 0))
    {
      pattern++;
      if (pattern > 2)
        pattern = 0;

      if (pattern == 0)
        delayTime = 1000; // slow
      else if (pattern == 1)
        delayTime = 500; // medium
      else
        delayTime = 200; // fast
    }

    led.setHigh();
    vTaskDelay(delayTime / portTICK_PERIOD_MS); // convert ms -> RTOS ticks

    led.setLow();
    vTaskDelay(delayTime / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  // configure the LED pin as output via my own driver
  led.init();

  // Create the binary semaphore, starts "empty"
  buttonSemaphore = xSemaphoreCreateBinary();

  // Internal pull-up enabled so the pin idles HIGH; pressing the button pulls it LOW, which will trigger the interrupt
  pinMode(21, INPUT_PULLUP);

  // Register buttonInterrupt() to run on every falling edge on pin 21
  attachInterrupt(digitalPinToInterrupt(21), buttonInterrupt, FALLING);

  // Start the LED task: 2048-byte stack, no parameter, priority 1, no handle stored
  xTaskCreate(ledTask, "LED Task", 2048, NULL, 1, NULL);
}

void loop()
{
}