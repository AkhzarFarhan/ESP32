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
time_t timestamp;
struct tm timeinfo;
Logger& logger = Logger::getInstance();


void setup()
{
    // Initialize serial communication
    Serial.begin(BAUD_RATE);
    logger.log("DHT Temperature and Humidity Sensor Test");
    logger.log("DHT Sensor with OLED Display");

    // Start the DHT sensor
    dht.begin();

    // Initialize the OLED display
    if (!display.begin(SSD1306_BLACK, OLED_ADDRESS))
    { // Default I2C address is 0x3C
        logger.log("SSD1306 allocation failed");
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
        logger.log("Connecting to WiFi...");
    }
    logger.log("Connected to WiFi");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    logger.log("Time sync started...");

    // Wait for time to synchronize
    if (!getLocalTime(&timeinfo))
    {
        logger.log("Failed to obtain time");
    }
    logger.log("Time synced");
    timestamp = time(nullptr);

    // Display startup message
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Booting..."));
    timestamp = time(nullptr);
    logger.log("Booting...");
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
        logger.log("Failed to read from DHT sensor!");
        // Display error message on OLED
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("Error reading sensor"));
        logger.log("Error in reading sensor");
        display.display();
        network.set(0, 0, 0);
        network.send();
        return;
    }

    // Send DHT data to Network
    timestamp = time(nullptr);
    network.set(tempC, tempF, humidity);
    // logger.log("DHT values set");
    network.send();
    // logger.log("DHT values sent");

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