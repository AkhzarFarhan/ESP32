#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include "driver/i2s.h"
#include "constant.h"
#include "buzzer.h"
#include "logger.h"
#include "network.h"


#define DHTTYPE DHT11 // DHT11 or DHT22
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Logger& logger = Logger::getInstance();
Buzzer& buzzer = Buzzer::getInstance();
Network& network = Network::getInstance();

float tempC, tempF, humidity;


void setup()
{
    Serial.begin(BAUD_RATE);
    logger.log({0, 0, 0, 0, "Setup started..."});
    network.send();
    buzzer.play(1000, 500);

    // Start the DHT sensor
    dht.begin();

    // Initialize the OLED display
    if (!display.begin(SSD1306_BLACK, OLED_ADDRESS))
    { // Default I2C address is 0x3C
        logger.log({0, 0, 0, 0, "SSD1306 allocation failed"});
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
        logger.log({0, 0, 0, 0, "Connecting to WiFi..."});
    }
    logger.log({0, 0, 0, 0, "Connected to WiFi"});

    // Display startup message
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Booting..."));
    logger.log({0, 0, 0, 0, "Booting..."});
    display.display();
    delay(SHORT_DELAY);

}

void loop()
{
    delay(SHORT_DELAY);
    buzzer.play(1000, 500);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(LONG_DELAY);
        logger.log({0, 0, 0, 0, "Suspected power cut. Connecting to WiFi..."});
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("Suspected power cut. Connecting to WiFi..."));
        display.display();
    }

    // Read temperature and humidity values
    humidity = dht.readHumidity();
    tempC = dht.readTemperature(); // Celsius
    tempF = dht.readTemperature(true); // Fahrenheit

    // Check if any reads failed
    if (isnan(humidity) || isnan(tempC) || isnan(tempF))
    {
        logger.log({0, 0, 0, 0, "Failed to read from DHT sensor!"});
        // Display error message on OLED
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("Error reading sensor"));
        logger.log({0, 0, 0, 0, "Error in reading sensor"});
        display.display();
        network.send();
        return;
    }

    // Send DHT data to Network
    network.send();

    // Print the results
    // Serial.print(F("Humidity: "));
    // Serial.print(humidity);
    // Serial.print(F("%  Temperature: "));
    // Serial.print(tempC);
    // Serial.print(F("°C "));
    // Serial.print(tempF);
    // Serial.println(F("°F"));

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
