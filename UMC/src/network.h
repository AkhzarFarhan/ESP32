#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>
#include "constant.h"
#include "logger.h"

extern Logger& logger;

class Network
{
    private:

        struct LogEntry
        {
            std::string timestamp;
            float tempC;
            float tempF;
            float humidity;
            std::string log;
        } log;
    public:

        Network() : log({"0.0", 0.0, 0.0, 0.0, "No log"}) {}
        ~Network() {}

        static Network& getInstance()
        {
            static Network instance;
            return instance;
        }

        String get_tg_url()
        {
            String s;
            s += String(TG_BASE);
            s += "*Timestamp: *";
            s += String(log.timestamp.c_str());
            s += "%0A*Temperature: *";
            s += String(log.tempC);
            s += "%0A*Humidity: *";
            s += String(log.humidity);
            s += "%0A*Log: *";
            s += String(log.log.c_str());
            s += "&parse_mode=markdown";
            return s;
        }

        void send()
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                HTTPClient http;
                
                // Send message to Telegram
                String tg_url = get_tg_url();
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