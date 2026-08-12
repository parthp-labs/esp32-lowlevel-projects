#include <Arduino.h>
#include "GPIODriver.hpp"

#define TRIG_PIN 26
#define ECHO_PIN 25

GPIODriver irSensor(22); // TCRT5000

void setup()
{
  Serial.begin(115200);

  irSensor.initInput();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);
}

float calculate_distance()
{
  // Trigger ultrasonic sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long init_time = micros();
  while (!digitalRead(ECHO_PIN))
  {
    if (micros() - init_time > 5000)
    {
      return -1;
    }
  };
  long start_time = micros();

  while (digitalRead(ECHO_PIN))
  {
    if (micros() - start_time > 60000)
    {
      return -1;
    }
  };

  long end_time = micros();

  long duration = end_time - start_time;

  return duration * 0.0343 / 2;
}

void loop()
{

  // Calculate distance in cm
  float distance = calculate_distance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  int isDetected = !irSensor.read();
  Serial.print("Detected: ");
  Serial.println(isDetected);

  delay(60);
}