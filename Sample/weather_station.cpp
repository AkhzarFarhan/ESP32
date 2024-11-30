#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

// Define the DHT sensor type and the GPIO pin
#define DHTPIN 4 // GPIO pin connected to the DHT sensor (change as needed)
#define DHTTYPE DHT11 // DHT11 or DHT22

// Initialize the DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// OLED display settings
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels
#define OLED_RESET -1 // Reset pin (set to -1 if not used)
#define OLED_ADDRESS 0x3C // I2C address for the OLED display (0x3C or 0x3D)

// Initialize the OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


void setup()
{
  // Initialize serial communication
    Serial.begin(115200);
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

    // Display startup message
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Booting..."));
    display.display();
    delay(2000);
}

void loop()
{
    // Wait a few seconds between measurements
    delay(2000);

    // Read temperature and humidity values
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // Celsius
    float temperatureF = dht.readTemperature(true); // Fahrenheit

    // Check if any reads failed
    if (isnan(humidity) || isnan(temperature) || isnan(temperatureF))
    {
        Serial.println(F("Failed to read from DHT sensor!"));
        // Display error message on OLED
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("Error reading sensor"));
        display.display();
        return;
    }

    // Print the results
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(temperature);
    Serial.print(F("°C "));
    Serial.print(temperatureF);
    Serial.println(F("°F"));

    // Display readings on OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("DHT Sensor Readings:"));
    display.setTextSize(2);
    display.setCursor(0, 16);
    display.print(F("T: "));
    display.print(temperature);
    display.println(F(" C"));
    display.setCursor(0, 40);
    display.print(F("H: "));
    display.print(humidity);
    display.println(F("%"));
    display.display();
}
