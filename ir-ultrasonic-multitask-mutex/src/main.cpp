#include <Arduino.h>
#include "GPIODriver.hpp"

#define TRIG_PIN 26
#define ECHO_PIN 25

GPIODriver irSensor(22);

SemaphoreHandle_t dataMutex;

struct SensorData
{
  bool obstacleDetected;
  float distanceCm;
};

SensorData data;

void irTask(void *parameter)
{
  while (1)
  {
    while (!xSemaphoreTake(dataMutex, portMAX_DELAY))
    {
    }
    data.obstacleDetected = !irSensor.read();
    xSemaphoreGive(dataMutex);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
};

void ultrasonicTask(void *parameter)
{
  while (1)
  {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);
    bool validReading = true;

    long init_time = micros();
    while (!digitalRead(ECHO_PIN))
    {
      if (micros() - init_time > 5000)
      {
        Serial.println("Unable to raise ECHO high");
        validReading = false;
        break; // exits the inner while loop
      }
    }

    long start_time;
    if (validReading)
    {
      start_time = micros();
      while (digitalRead(ECHO_PIN))
      {
        if (micros() - start_time > 60000)
        {
          Serial.println("No object detected");
          validReading = false;
          break;
        }
      }
    }

    if (validReading)
    {
      long end_time = micros();
      long duration = end_time - start_time;
      long distance = duration * 0.0343 / 2;

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      data.distanceCm = distance;
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
};

void monitorTask(void *parameter)
{
  while (1)
  {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    Serial.print("Object Detected: ");
    Serial.println(data.obstacleDetected);
    Serial.print("Distance: ");
    Serial.println(data.distanceCm);
    xSemaphoreGive(dataMutex);
    vTaskDelay(100 / portTICK_PERIOD_MS);
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
  xTaskCreate(irTask, "IR Task", 2048, NULL, 1, NULL);
  xTaskCreate(ultrasonicTask, "Ultrasonic Task", 2048, NULL, 1, NULL);
  xTaskCreate(monitorTask, "Monitor Task", 2048, NULL, 1, NULL);
}

void loop()
{
}
