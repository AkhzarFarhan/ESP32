#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>
#include "constant.h"
#include "logger.h"

extern Logger& logger;
extern time_t timestamp;

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
                
                // Send message to Telegram
                String tg_url = String(TG_BASE) + "*Timestamp: *" + String(timestamp) + "%0A*Temperature: *" + String(tempC) + "%0A*Humidity: *" + String(humidity) + "%0A*Log: *" + String(log.c_str());
                tg_url += "&parse_mode=markdown";

                http.begin(tg_url);
                int httpResponseCode = http.GET();

                if (httpResponseCode > 0)
                {
                    String response = http.getString();
                    //logger.log("Telegram Response code: " + String(httpResponseCode));
                    //logger.log("Response: " + response);
                }
                else
                {
                    //logger.log("Error on sending GET to Telegram: " + String(httpResponseCode));
                }

                http.end(); // Free resources

                /*
                http.begin(API_ENDPOINT);

                http.addHeader("Content-Type", "application/json");

                // Create JSON object
                StaticJsonDocument<200> jsonDoc;
                jsonDoc["temperatureC"] = tempC;
                jsonDoc["temperatureF"] = tempF;
                jsonDoc["humidity"] = humidity;
                jsonDoc["log"] = log;
                jsonDoc["timestamp"] = timestamp;

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
                */
            }
            else
            {
                Serial.println("WiFi not connected");
            }
        }
        void receive() {}
};

#endif // NETWORK_H