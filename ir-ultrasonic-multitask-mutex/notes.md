Weekend 2 Summary — IR + Ultrasonic Multitask with Mutex
What I built

I built a system with two sensor drivers — a digital IR obstacle sensor (TCRT5000) and an ultrasonic distance sensor (HC-SR04) — running as separate FreeRTOS tasks. Both write their readings into a shared struct that's protected by a mutex, and a third task reads that struct and prints combined status. I also upgraded the ultrasonic measurement from a busy-wait loop to a full interrupt-driven implementation.

Core Concepts Learned

1. Reading GPIO via registers

I extended my GPIODriver to support input reads using GPIO_IN_REG (0x3ff4403A), following the same shift-and-mask pattern I already knew from the output side: read the whole 32-bit register, shift right by the pin number, mask with & 1 to isolate that one bit.

2. Active-LOW sensor logic

My TCRT5000 module reads 1 when clear and 0 when an object is detected — the opposite of what feels intuitive. I learned to invert this (!irSensor.read()) at the sensor-semantics layer rather than inside GPIODriver itself, so the raw register driver stays generic and the sensor-specific quirk lives in exactly one place.

3. Voltage-level safety with real hardware

ESP32 GPIO pins aren't 5V tolerant, but HC-SR04 is officially a 5V part. I learned that powering it at 5V means its Echo output also swings up to 5V, which needs a voltage divider (I used 10kΩ/20kΩ, a 1:2 ratio) to bring it down to a safe ~3.3V before it reaches a GPIO pin. I also explored running the sensor at 3.3V directly (no divider) as a simpler but less officially supported option, and ended up going with 3.3V-direct for this build.

4. Hand-rolled pulse-width timing (busy-wait)

Instead of using pulseIn(), I wrote my own two-stage wait: block until Echo goes HIGH (start the clock), block until it goes LOW again (stop the clock), then calculate duration \* 0.0343 / 2 for distance in cm. I learned the hard way that millis() is far too coarse for this — pulse widths are sub-millisecond, so I needed micros() instead.

5. Timeout guards on blocking waits

Any loop waiting for a pin to change state needs a way out — without a timeout, a missing echo (nothing in range, sensor disconnected) hangs the loop forever. I added independent timeout checks to both the rising-edge wait and the falling-edge wait.

6. FreeRTOS multitasking with shared state

I ran three tasks concurrently (irTask, ultrasonicTask, monitorTask), all touching a shared SensorData struct, protected with a mutex (xSemaphoreCreateMutex, xSemaphoreTake/xSemaphoreGive). The big lesson here: only hold the lock around the actual read/write of the shared struct, not around slow work like sensor polling or Serial.print — otherwise the tasks end up serialized instead of actually running concurrently.

7. Interrupt-based pulse timing

I replaced the busy-wait with a CHANGE-triggered interrupt on the Echo pin. The ISR figures out whether it's a rising or falling edge by reading the pin state: on rising edge it stores a volatile timestamp, on falling edge it computes the pulse width and signals a semaphore. ultrasonicTask now blocks on that semaphore (with a timeout) instead of polling, so the CPU is free during the wait instead of spinning.

Mistakes I Caught and Fixed
delay(0.01) instead of delayMicroseconds(10) — delay() only works in whole milliseconds, so this was effectively producing a zero-length trigger pulse. I learned delay() and microsecond timing are completely different tools.
millis() used to time a sub-millisecond pulse — this always returned 0, since the whole pulse finished faster than one millisecond tick. Switched to micros().
continue inside a nested while timeout branch, more than once — I kept jumping back to the inner loop instead of exiting it, which created infinite loops with no vTaskDelay. The first version caused a mutex deadlock (a task held the lock while spinning forever). A later version caused an ESP32 task watchdog reset (the task never yielded the CPU at all). I fixed this by replacing continue with break plus a validReading flag, making sure every path — success or timeout — reaches vTaskDelay.
Stack overflow crash (Guru Meditation Error... Stack canary watchpoint triggered) — my task stacks were only 255 bytes, way too small for the actual call chain (sensor reads, Serial, FreeRTOS's own bookkeeping). I fixed it by bumping stack sizes to 2048 bytes. The real lesson: an undersized stack doesn't always fail immediately or obviously — FreeRTOS's canary caught it here, but without RTOS protection this could have been silent corruption instead, exactly like the very first notes I took on bare-metal systems warned about.
Mutex created after the tasks that use it — since tasks can start running the instant xTaskCreate is called, dataMutex needs to exist before any task might reference it. Fixed by reordering setup().
Holding the mutex across the entire slow sensor-reading sequence instead of just the final struct write — this defeated the purpose of having separate concurrent tasks, since it effectively serialized them. Fixed by narrowing the lock scope to just the assignment into the shared struct.
First interrupt-based draft busy-waited inside the ISR itself, and called non-ISR-safe functions (xSemaphoreTake/Give, Serial.println) from interrupt context — a real violation of "ISRs must stay short and non-blocking." I fixed this by moving all computation and mutex work back into the task, leaving the ISR to only record a timestamp or signal a semaphore.
Interrupt initially attached on FALLING only — this missed the rising edge entirely. Fixed by switching to CHANGE and reading the pin state inside the ISR to tell the two edges apart.
Known Limitation — Noted, Not Fixed

ultrasonicTask's timeout on xSemaphoreTake (70ms) and the ISR's own reliance on echoStartTime being set correctly assume a clean rising-then-falling sequence per cycle. I haven't tested what happens if the interrupt somehow misses an edge (e.g. two rising edges in a row due to electrical noise) — this could leave pulseWidth reflecting a stale or incorrect measurement without any error being raised. Worth stress-testing or adding a sanity check later.
