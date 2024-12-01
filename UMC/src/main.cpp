#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "network.h"
#include "constant.h"
#include "logger.h"


#define DHTTYPE DHT11 // DHT11 or DHT22
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Network network;
float tempC, tempF, humidity;
time_t timestamp = time(nullptr);



void setup()
{
    // Initialize serial communication
    Serial.begin(BAUD_RATE);
    Serial.println(F("DHT Temperature and Humidity Sensor Test"));
    Serial.println(F("DHT Sensor with OLED Display"));

    // Start the DHT sensor
    dht.begin();

    // Initialize the OLED display
    if (!display.begin(SSD1306_BLACK, OLED_ADDRESS))
    { // Default I2C address is 0x3C
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Halt the program
    }

    // Clear the display
    display.clearDisplay();
    display.display();

        // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(DELAY);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Display startup message
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Booting..."));
    display.display();
    delay(DELAY);
}

void loop()
{
    // Wait a few seconds between measurements
    delay(DELAY);

    // Read temperature and humidity values
    humidity = dht.readHumidity();
    tempC = dht.readTemperature(); // Celsius
    tempF = dht.readTemperature(true); // Fahrenheit

    // Check if any reads failed
    if (isnan(humidity) || isnan(tempC) || isnan(tempF))
    {
        Serial.println(F("Failed to read from DHT sensor!"));
        // Display error message on OLED
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("Error reading sensor"));
        display.display();
        network.set(0, 0, 0);
        network.send();
        return;
    }

    // Send DHT data to Network
    network.set(tempC, tempF, humidity);
    network.send();

    // Print the results
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(tempC);
    Serial.print(F("°C "));
    Serial.print(tempF);
    Serial.println(F("°F"));

    // Display readings on OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("DHT Sensor Readings:"));
    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print(F("T: "));
    display.print(tempC);
    display.println(F(" C"));
    display.setCursor(0, 40);
    display.print(F("H: "));
    display.print(humidity);
    display.println(F("%"));
    display.display();
}