#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "JioFiber_401_2.4Gz";
const char *password = "Melvin420";

WebServer server(80);

void handleRoot() {
    server.send(200, "text/html", "<h1>Hello, World!</h1>");
}

void setup() {
    Serial.begin(9600);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
    server.on("/", handleRoot);
    server.begin();
}

void loop() {
    server.handleClient();
    Serial.println("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
}
