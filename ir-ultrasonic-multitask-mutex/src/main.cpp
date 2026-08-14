#include <Arduino.h>
#include "GPIODriver.hpp"

#define TRIG_PIN 26
#define ECHO_PIN 25

GPIODriver irSensor(22);

// Protects the shared SensorData struct (used by irTask, ultrasonicTask, monitorTask)
SemaphoreHandle_t dataMutex;

// Signals ultrasonicTask that echoInterrupt has finished timing a pulse
SemaphoreHandle_t echoSemaphore;

struct SensorData
{
  bool obstacleDetected;
  float distanceCm;
};

SensorData data;

// Written only by the ISR, read only by ultrasonicTask after it wakes up —
// no mutex needed since there's no concurrent access from multiple tasks.
volatile uint32_t echoStartTime = 0;
volatile uint32_t pulseWidth = 0;

void IRAM_ATTR echoInterrupt()
{
  if (digitalRead(ECHO_PIN) == HIGH)
  {
    // Rising edge — pulse just started, record when
    echoStartTime = micros();
  }
  else
  {
    // Falling edge — pulse just ended, compute width and wake the task
    pulseWidth = micros() - echoStartTime;

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(echoSemaphore, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  }
}

void irTask(void *parameter)
{
  while (1)
  {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    data.obstacleDetected = !irSensor.read();
    xSemaphoreGive(dataMutex);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void ultrasonicTask(void *parameter)
{
  while (1)
  {
    // --- Trigger sequence
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    // --- Wait for the ISR to signal that a full pulse was measured ---
    // Timeout ~70ms: generous enough to cover the ~60ms max echo window
    // plus a little slack for the initial rising edge to occur.
    if (xSemaphoreTake(echoSemaphore, pdMS_TO_TICKS(70)))
    {
      float distance = pulseWidth * 0.0343f / 2.0f;

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      data.distanceCm = distance;
      xSemaphoreGive(dataMutex);
    }
    else
    {
      Serial.println("Echo timeout - no pulse detected");
      // data.distanceCm intentionally left unchanged on timeout
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void monitorTask(void *parameter)
{
  while (1)
  {
    bool obstacle;
    float distance;

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    obstacle = data.obstacleDetected;
    distance = data.distanceCm;
    xSemaphoreGive(dataMutex);

    Serial.print("Object Detected: ");
    Serial.print(obstacle);
    Serial.print(" | Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);

  irSensor.initInput();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  dataMutex = xSemaphoreCreateMutex();
  echoSemaphore = xSemaphoreCreateBinary();

  // CHANGE, not FALLING — we need both edges to time the pulse width
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoInterrupt, CHANGE);

  xTaskCreate(irTask, "IR Task", 2048, NULL, 1, NULL);
  xTaskCreate(ultrasonicTask, "Ultrasonic Task", 2048, NULL, 1, NULL);
  xTaskCreate(monitorTask, "Monitor Task", 2048, NULL, 1, NULL);
}

void loop()
{
}