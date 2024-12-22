#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include <cmath>
#include <WiFi.h>
 #include <WiFiClientSecure.h>


// Create a Bluetooth A2DP source object
BluetoothA2DPSource a2dp_source;

// Define constants for sine wave generation
const float frequency = 440.0; // A4 (440 Hz)
const float sampleRate = 44100.0; // Standard audio sample rate
const int16_t amplitude = 30000; // Amplitude of the sine wave
float phase = 0.0;

// Callback to generate sound data
int32_t get_sound_data(Frame* data, int32_t frameCount) {
    for (int32_t i = 0; i < frameCount; i++) {
        int16_t sampleValue = (int16_t)(amplitude * sinf(phase)); // Generate sine wave sample
        data[i] = Frame(sampleValue, sampleValue); // Assign the same value to both channels
        phase += (2.0f * M_PI * frequency) / sampleRate; // Increment phase
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI; // Wrap phase
    }

    return frameCount; // Return the number of frames generated
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Bluetooth A2DP...");

    // Initialize Bluetooth A2DP source with callback
    a2dp_source.start("Flip 6", get_sound_data);

    Serial.println("Bluetooth A2DP started. Connect your speaker!");
}

void loop() {
    // Nothing needed here; audio is streamed in the background
}