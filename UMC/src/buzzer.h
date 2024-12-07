#ifndef BUZZER_H
#define BUZZER_H

#include <esp32-hal-ledc.h>
#include <esp32-hal.h>
#include "constant.h"

class Buzzer
{
public:
    static Buzzer& getInstance()
    {
        static Buzzer instance;
        return instance;
    }

    void play(int frequency, int duration)
    {
        pinMode(BUZZER_PIN, OUTPUT);
        ledcSetup(0, frequency, 8); // Channel 0, frequency, 8-bit resolution
        ledcAttachPin(BUZZER_PIN, 0);
        ledcWrite(0, 127); // 50% duty cycle
        delay(duration);
        ledcWrite(0, 0); // Stop the tone
    }

private:
    Buzzer() {} // Private constructor
    Buzzer(const Buzzer&) = delete;
    Buzzer& operator=(const Buzzer&) = delete;
};

#endif // BUZZER_H