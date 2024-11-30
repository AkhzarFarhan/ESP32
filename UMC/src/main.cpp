#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>


// Define the DHT sensor type and the GPIO pin
#define DHTPIN 4 // GPIO pin connected to the DHT sensor (change as needed)
#define DHTTYPE DHT11 // DHT11 or DHT22
DHT dht(DHTPIN, DHTTYPE);


// OLED display settings
#define SCREEN_WIDTH 128 // OLED display width in pixels
#define SCREEN_HEIGHT 64 // OLED display height in pixels
#define OLED_RESET -1 // Reset pin (set to -1 if not used)
#define OLED_ADDRESS 0x3C // I2C address for the OLED display (0x3C or 0x3D)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


class OLED_Display
{
    private:
        /* data */
    public:
        OLED_Display(/* args */);
        ~OLED_Display();
};

OLED_Display::OLED_Display(/* args */)
{
}

OLED_Display::~OLED_Display()
{
}



void setup()
{

}

void loop()
{

}