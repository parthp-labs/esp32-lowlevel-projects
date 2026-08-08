# Weekend 1 Summary — Bare-Metal GPIO Driver + Debounced Interrupt

## What I built

I built a register-level GPIO driver (`GPIODriver`) that controls an LED directly using `GPIO_ENABLE_REG` and `GPIO_OUT_REG`. I also implemented an interrupt-driven button that cycles through 3 different blink-speed patterns using the correct **ISR → semaphore → task** handoff pattern.

## Core Concepts Learned

### 1. `digitalWrite()` / `pinMode()` are just wrappers

There’s no magic behind these functions. They eventually perform the same register and bitmask operations that I already knew from my notes.

Writing my own `GPIODriver` helped me understand what is actually happening instead of just reading about it.

### 2. ISR → Task Handoff Pattern

ISRs should be kept short and non-blocking. They should not perform the actual work, such as changing the LED or handling pattern logic.

The correct approach is:

**ISR → signal semaphore → FreeRTOS task does the actual work**

`xSemaphoreGiveFromISR(sem, &flag)` signals the semaphore and also tells us, through `flag`, whether giving the semaphore unblocked a higher-priority task.

`portYIELD_FROM_ISR(flag)` should be called at the end of the ISR to immediately switch to that higher-priority task instead of waiting for the next scheduler tick.

This was one of the most important things I learned because it is easy to forget when using the `FromISR` APIs. The code can still appear to work without it, but it can introduce unnecessary latency.

### 3. Debouncing is a real, non-trivial problem

A single physical button press can cause multiple rapid electrical transitions because of contact bounce. Without debouncing, this can cause the ISR to trigger multiple times for a single press.

I fixed this using a timestamp-based guard that rejects any trigger occurring within **200 ms** of the last accepted trigger.

This wasn't really noticeable in simulation or theory. I only properly understood the problem after testing it on actual hardware.

### 4. `IRAM_ATTR`

`IRAM_ATTR` places the ISR code in internal RAM instead of flash.

This ensures that the ISR can still execute when flash access is temporarily unavailable, such as during certain flash write operations. It is an ESP32-specific requirement for interrupt code that needs to remain available during these situations.

### 5. Non-blocking semaphore check in a running task

`xSemaphoreTake(sem, 0)` uses a timeout of zero, which means the task checks whether the button has triggered the semaphore without ever blocking.

This allows `ledTask` to check:

> "Did the button fire since I last checked?"

without stopping the LED blink loop while waiting for the button.

## Mistakes I Caught and Fixed

- **Missing `portYIELD_FROM_ISR` in the first draft** — The semaphore was being given, but the higher-priority task was not being scheduled immediately, which could introduce unnecessary latency.

- **Unused `volatile` variables** — `status`, `duration`, and `buttonPressed` were left over from earlier code and were not being used. I removed them because unused state can make future debugging more confusing.

- **Unused `mask` field in the driver header** — This was also leftover state that wasn't actually needed.

- **`GPIODriver.cpp` was accidentally a duplicate of `main.cpp`** — I caught this before it caused more confusion and replaced it with the actual driver implementation.

- **`Serial.println()` inside the ISR** — Technically, this isn't guaranteed to be interrupt-safe. I kept it temporarily for debugging, but it should be removed once the debounce logic is verified.

## Known Limitation — To Fix Later

My current `GPIODriver` only supports GPIO pins **0–31** because `GPIO_ENABLE_REG` and `GPIO_OUT_REG` cover those pins.

The ESP32 also has a second register pair, `GPIO_ENABLE1_REG` and `GPIO_OUT1_REG`, for GPIO pins **32 and above**.

So, if I extend the driver to support higher GPIO numbers, the current implementation will silently fail. I should add a comment about this in the header and possibly implement support for the second register pair as a future exercise.
