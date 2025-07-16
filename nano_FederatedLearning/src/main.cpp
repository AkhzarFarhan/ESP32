#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>



class FederatedClient
{
private:
    const char* ssid;
    const char* password;
    const char* serverUrl;
    float weight;
    float bias;
    float localData[5] = {1.0, 2.0, 3.0, 4.0, 5.0};  // dummy x
    float localLabel[5] = {2.2, 4.1, 6.3, 8.05, 10.2}; // dummy y

public:
    FederatedClient(const char* wifiSsid, const char* wifiPassword, const char* url)
    {
        ssid = wifiSsid;
        password = wifiPassword;
        serverUrl = url;
        weight = 0.0f;
        bias = 0.0f;
    }

    void connectWiFi()
    {
      WiFi.begin(ssid, password);
      Serial.print("Connecting to WiFi");
      while (WiFi.status() != WL_CONNECTED)
      {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nWiFi connected.");
    }

    void localTrain()
    {
        // One step of gradient descent for linear regression (federated client)
        float dw = 0.0f, db = 0.0f, lr = 0.01f;
        int n = sizeof(localData) / sizeof(localData[0]);
        for (int i = 0; i < n; i++)
        {
            float x = localData[i];
            float y = localLabel[i];
            float pred = weight * x + bias;
            dw += 2.0f * (pred - y) * x; // dL/dw for MSE
            db += 2.0f * (pred - y);     // dL/db for MSE
        }
        weight -= lr * dw / n;
        bias   -= lr * db / n;
    }

    void sendModel()
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<200> doc;
        doc["client_id"] = "esp32_1";
        doc["weight"] = weight;
        doc["bias"] = bias;
        String output;
        serializeJson(doc, output);

        int httpResponseCode = http.POST(output);
        Serial.printf("POST Response: %d\n", httpResponseCode);
        http.end();
      }
    }

    void receiveUpdatedModel()
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        HTTPClient http;
        http.begin(serverUrl);  // GET same URL
        int httpCode = http.GET();

        if (httpCode == 200)
        {
          String payload = http.getString();
          StaticJsonDocument<200> doc;
          DeserializationError error = deserializeJson(doc, payload);
          if (!error)
          {
            weight = doc["weight"];
            bias = doc["bias"];
            Serial.println("Received new model.");
          }
        }
        http.end();
      }
    }

    void printModel()
    {
      Serial.printf("Model: y = %.3fx + %.3f\n", weight, bias);
    }
};



void setup()
{

}

void loop()
{

}

