#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include "driver/i2s.h"

#define BUZZER_PIN 25

void playTone(int frequency, int duration)
{
    ledcSetup(0, frequency, 8); // Channel 0, frequency, 8-bit resolution
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 127); // 50% duty cycle
    delay(duration);
    ledcWrite(0, 0); // Stop the tone
}

void setup()
{
    Serial.begin(115200);
    pinMode(BUZZER_PIN, OUTPUT);
    playTone(1000, 500); // Play a startup tone
}

void loop()
{
    delay(10000);
    playTone(1000, 10);
    delay(10000);
}
