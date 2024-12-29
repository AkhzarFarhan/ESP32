#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Replace with your Wi-Fi credentials
const char* ssid = "JioFiber_401_2.4Gz";
const char* password = "Melvin420";

// Replace with your Last.fm API key
const String apiKey = "your_lastfm_api_key";

// Last.fm API URL for fetching the trending tracks
const String lastFMUrl = "https://ws.audioscrobbler.com/2.0/?method=chart.gettoptracks&api_key=" + apiKey + "&format=json";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to Wi-Fi");

  // Fetch and display trending songs
  fetchTrendingSongs();
}

void loop() {
  // You can set up periodic fetching of song data if needed
}

// Function to fetch trending songs from Last.fm
void fetchTrendingSongs() {
  HTTPClient http;
  http.begin(lastFMUrl);  // Start the HTTP request

  // Send GET request
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();  // Get the response as a string

    // Parse the JSON response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    // Print the top tracks
    JsonArray tracks = doc["tracks"]["track"];
    Serial.println("Top Tracks from Last.fm:");

    for (JsonObject track : tracks) {
      String trackName = track["name"];
      String artistName = track["artist"]["name"];
      String url = track["url"];

      Serial.println("Track: " + trackName);
      Serial.println("Artist: " + artistName);
      Serial.println("Listen: " + url);
      Serial.println("-----------");
    }
  } else {
    Serial.println("Failed to fetch data from Last.fm");
  }

  http.end();  // End the HTTP connection
}
