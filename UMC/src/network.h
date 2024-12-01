#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>
#include "constant.h"
#include "logger.h"

Logger logger;

class Network
{
    private:
        float tempC;
        float tempF;
        float humidity;
        std::string log;
    public:
        Network() : tempC(0.0), tempF(0.0), humidity(0.0), log("No log") {}
        ~Network() {}

        void set(float tempC, float tempF, float humidity)
        {
            this->tempC = tempC;
            this->tempF = tempF;
            this->humidity = humidity;
            logger.getLog(log);
        }
        void reset()
        {
            this->tempC = 0;
            this->tempF = 0;
            this->humidity = 0;
        }
        void send()
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                HTTPClient http;
                http.begin(API_ENDPOINT);

                http.addHeader("Content-Type", "application/json");

                // Create JSON object
                StaticJsonDocument<200> jsonDoc;
                jsonDoc["temperatureC"] = tempC;
                jsonDoc["temperatureF"] = tempF;
                jsonDoc["humidity"] = humidity;
                jsonDoc["log"] = log;

                // Serialize JSON object to string
                String jsonString;
                serializeJson(jsonDoc, jsonString);

                // Send HTTP POST request
                int httpResponseCode = http.POST(jsonString);

                if (httpResponseCode > 0)
                {
                    String response = http.getString();
                    Serial.println(httpResponseCode);
                    Serial.println(response);
                }
                else
                {
                    Serial.print("Error on sending POST: ");
                    Serial.println(httpResponseCode);
                }

                http.end(); // Free resources
            }
            else
            {
                Serial.println("WiFi not connected");
            }
        }
        void receive() {}
};

#endif // NETWORK_H